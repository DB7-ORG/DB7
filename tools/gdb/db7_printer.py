# GDB pretty-printers for db7 catalog / dependency objects (libstdc++).
# Load:    source /path/to/db7_printers.py   (or via launch.json setupCommands)
# Reload:  -exec source /path/to/db7_printers.py   (VS Code Debug Console)
# Disable temporarily:  -exec disable pretty-printer global

import itertools

import gdb

# --- adjust if your names differ ---------------------------------------------
NS = "db7::catalog::"
OPTIONAL_PTR_FIELD = "ptr"            # raw pointer inside db7::optional_ptr<T>
PHYSICAL_INDEX_FIELD = "index"        # integer inside PhysicalIndex
COLUMN_LIST_FIELDS = ("columns",)     # vector<ColumnDefinition> inside ColumnList
COLUMN_NAME_FIELDS = ("name",)        # name inside ColumnDefinition
COLUMN_TYPE_FIELDS = ("type",)        # type inside ColumnDefinition

# --- safety limits: printers also run on uninitialized locals full of garbage --
MAX_VERSIONS = 50      # version-chain links followed per catalog entry
MAX_ELEMENTS = 200     # container elements read per container
MAX_STRING = 4096      # bytes read per string
# -----------------------------------------------------------------------------


# ---------- generic helpers --------------------------------------------------

def _strip(t):
    t = t.strip_typedefs()
    if t.code in (gdb.TYPE_CODE_REF, gdb.TYPE_CODE_RVALUE_REF):
        t = t.target().strip_typedefs()
    return t.unqualified()


def _deref(v):
    if v.type.strip_typedefs().code in (gdb.TYPE_CODE_REF, gdb.TYPE_CODE_RVALUE_REF):
        return v.referenced_value()
    return v


def _derives_from(t, tag):
    t = _strip(t)
    if t.code != gdb.TYPE_CODE_STRUCT:
        return False
    if t.tag == tag:
        return True
    return any(f.is_base_class and _derives_from(f.type, tag) for f in t.fields())


def _get(v, *names):
    """First field that exists, or None."""
    for n in names:
        try:
            return v[n]
        except gdb.error:
            pass
    return None


def _dynamic(v):
    try:
        return v.cast(v.dynamic_type)
    except gdb.error:
        return v


def _iter_std(v, limit=MAX_ELEMENTS):
    """Lazily yield up to `limit` elements of a std container (libstdc++ printers)."""
    try:
        vis = gdb.default_visualizer(v)
        if vis is None or not hasattr(vis, "children"):
            return
        for _, c in itertools.islice(vis.children(), limit):
            yield c
    except Exception:
        return


def _iter_pairs(v, limit=MAX_ELEMENTS):
    """std::map -> lazily yield (key, value)."""
    it = _iter_std(v, limit * 2)
    for key in it:
        val = next(it, None)
        if val is None:
            return
        yield key, val


def _count(iterable, limit=MAX_ELEMENTS):
    n = sum(1 for _ in iterable)
    return f"{n}+" if n >= limit else str(n)


def _unique_ptr_get(v):
    for path in (("_M_t", "_M_t", "_M_head_impl"), ("_M_t", "_M_head_impl")):
        try:
            x = v
            for p in path:
                x = x[p]
            return x
        except gdb.error:
            pass
    return next(_iter_std(v, 1), None)


def _is_null(p):
    try:
        return p is None or int(p) == 0
    except Exception:
        return True


def _pointee(up):
    """unique_ptr -> object, or the string 'null'."""
    p = _unique_ptr_get(up)
    return "null" if _is_null(p) else p.dereference()


def _atomic(v):
    try:
        v = v["_M_i"]
    except gdb.error:
        pass
    try:
        return str(int(v))
    except Exception:
        return str(v)


def _string_bytes(s):
    """Raw bytes of a std::string (embedded \\0 kept), capped at MAX_STRING."""
    try:
        n = int(s["_M_string_length"])
        p = int(s["_M_dataplus"]["_M_p"])
    except gdb.error:
        return str(s).strip('"').encode()
    if n <= 0:
        return b""
    if n > MAX_STRING:  # garbage length in uninitialized memory, or just huge
        return b"<invalid or too long>"
    try:
        return bytes(gdb.selected_inferior().read_memory(p, n))
    except gdb.error:
        return b"<unreadable>"


def _enum(v, *strip):
    s = str(v).split("::")[-1]
    for x in strip:
        s = s.replace(x, "")
    return s


def _fmt_entry(type_, schema, name):
    return f"{type_} {schema}.{name}"


def _ident_bytes(v):
    return _string_bytes(_deref(v)["value"])


def _fmt_ident(raw):
    parts = raw.decode(errors="replace").split("\0")
    if len(parts) == 3:  # mangled entry: type\0schema\0name
        return _fmt_entry(*parts)
    if len(parts) == 6:  # mangled dependency: entry\0entry
        return f"{_fmt_entry(*parts[:3])} -> {_fmt_entry(*parts[3:])}"
    return "\\0".join(parts)


def _ident(v):
    return _fmt_ident(_ident_bytes(v))


def _text(v):
    """Identifier, std::string, enum or anything else -> short text."""
    if v is None:
        return "?"
    t = _strip(v.type)
    if t.code == gdb.TYPE_CODE_STRUCT and t.tag == "db7::Identifier":
        return _ident(v)
    if t.code == gdb.TYPE_CODE_STRUCT and t.tag and "basic_string" in t.tag:
        return _string_bytes(_deref(v)).decode(errors="replace")
    if t.code == gdb.TYPE_CODE_ENUM:
        return _enum(v)
    return str(v)


def _info(v):
    return f'{_enum(v["type"], "_ENTRY")} {_ident(v["schema"])}.{_ident(v["name"])}'


def _entry_summary(v):
    s = f'{_enum(v["type"], "_ENTRY")} {_ident(v["name"])}  ts={_atomic(v["timestamp"])}'
    if bool(v["deleted"]):
        s += "  [deleted]"
    return s


def _flat_fields(v, depth=0):
    """(name, value) for all data members, base classes flattened inline."""
    if depth > 10:
        return
    for f in v.type.strip_typedefs().fields():
        if f.is_base_class:
            yield from _flat_fields(v[f], depth + 1)
        elif not hasattr(f, "bitpos") or f.artificial or f.name.startswith("_vptr"):
            continue  # static members, vtable pointer
        else:
            yield f.name, v[f.name]


def _safe(fn):
    """Wrap to_string: show the error instead of breaking the Watch panel."""
    def wrapper(self):
        try:
            return fn(self)
        except Exception as e:
            return f"<error: {e}>"
    return wrapper


def _safe_children(fn):
    """Wrap children: stop quietly on errors in garbage memory."""
    def wrapper(self):
        try:
            yield from fn(self)
        except Exception as e:
            yield "<error>", str(e)
    return wrapper


# ---------- names and small values -------------------------------------------

class IdentifierPrinter:
    def __init__(self, v): self.v = v
    @_safe
    def to_string(self): return _ident(self.v)


class MangledNamePrinter:
    def __init__(self, v): self.v = v
    @_safe
    def to_string(self): return _ident(self.v["name"])


class CatalogEntryInfoPrinter:
    def __init__(self, v): self.v = v
    @_safe
    def to_string(self): return _info(self.v)


class OptionalPtrPrinter:
    def __init__(self, v):
        self.p = _get(v, OPTIONAL_PTR_FIELD)
        if self.p is None:
            self.p = v[v.type.strip_typedefs().fields()[0]]
    @_safe
    def to_string(self): return "null" if _is_null(self.p) else str(self.p)
    @_safe_children
    def children(self):
        if not _is_null(self.p):
            yield "*", self.p.dereference()


# ---------- dependencies -----------------------------------------------------

class LogicalDependencyPrinter:
    def __init__(self, v): self.v = v
    @_safe
    def to_string(self):
        catalog = _ident(self.v["catalog"])
        return _info(self.v["entry"]) + (f"  @{catalog}" if catalog else "")


class LogicalDependencyListPrinter:
    def __init__(self, v): self.v = v
    @_safe
    def to_string(self): return f'{_count(_iter_std(self.v["set"]))} dependencies'
    @_safe_children
    def children(self):
        for i, item in enumerate(_iter_std(self.v["set"])):
            yield f"[{i}]", item
    def display_hint(self): return "array"


class DependencyCatalogSetPrinter:
    """Only this object's edges: keys starting with '{mangled_name}\\0'."""
    def __init__(self, v): self.v = v

    def _edges(self):
        prefix = _ident_bytes(self.v["mangled_name"]["name"]) + b"\0"
        entries = _deref(self.v["set"])["map"]["entries"]
        for key, up in _iter_pairs(entries):
            raw = _ident_bytes(key)
            if raw.startswith(prefix):
                yield raw, up

    @_safe
    def to_string(self):
        return f'{_info(self.v["info"])}: {_count(self._edges())} edge(s)'

    @_safe_children
    def children(self):
        for raw, up in self._edges():
            other = raw.split(b"\0")[3:]
            label = (_fmt_entry(*(x.decode(errors="replace") for x in other))
                     if len(other) == 3 else _fmt_ident(raw))
            yield label, _pointee(up)


# ---------- catalog entries --------------------------------------------------

class CatalogEntryPrinter:
    """One line per entry; older MVCC versions listed as children."""
    def __init__(self, v): self.v = v

    def _older(self):
        p = _unique_ptr_get(self.v["child"])
        for _ in range(MAX_VERSIONS):
            if _is_null(p):
                return
            e = p.dereference()
            yield e
            p = _unique_ptr_get(e["child"])

    @_safe
    def to_string(self):
        n = sum(1 for _ in self._older())
        return _entry_summary(self.v) + (f"  (+{n} older)" if n else "")

    @_safe_children
    def children(self):
        for i, e in enumerate(self._older()):
            yield f"v-{i + 1}", _entry_summary(e)


class CatalogEntryMapPrinter:
    def __init__(self, v): self.v = v
    @_safe
    def to_string(self): return f'{_count(_iter_pairs(self.v["entries"]))} entries'
    @_safe_children
    def children(self):
        for key, up in _iter_pairs(self.v["entries"]):
            yield "key", key
            yield "value", _pointee(up)
    def display_hint(self): return "map"


# ---------- columns and constraints ------------------------------------------

class ColumnDefinitionPrinter:
    def __init__(self, v): self.v = v
    @_safe
    def to_string(self):
        return f'{_text(_get(self.v, *COLUMN_NAME_FIELDS))} {_text(_get(self.v, *COLUMN_TYPE_FIELDS))}'


class ColumnListPrinter:
    def __init__(self, v): self.cols = _get(v, *COLUMN_LIST_FIELDS)
    @_safe
    def to_string(self): return f"{_count(_iter_std(self.cols))} columns"
    @_safe_children
    def children(self):
        for i, col in enumerate(_iter_std(self.cols)):
            yield f"[{i}]", col
    def display_hint(self): return "array"


def _phys(v):
    try:
        return str(int(v[PHYSICAL_INDEX_FIELD]))
    except Exception:
        return str(v)


class ForeignKeyInfoPrinter:
    def __init__(self, v): self.v = v
    @_safe
    def to_string(self):
        v = self.v
        pk = ",".join(_phys(x) for x in _iter_std(v["pk_keys"]))
        fk = ",".join(_phys(x) for x in _iter_std(v["fk_keys"]))
        return (f'{_enum(v["type"], "FK_TYPE_")} {_ident(v["schema"])}.{_ident(v["table"])}'
                f"  pk=[{pk}] fk=[{fk}]")


def _columns(vec):
    return ", ".join(_ident(c) for c in _iter_std(vec))


class ConstraintPrinter:
    def __init__(self, v): self.v = _dynamic(v)
    @_safe
    def to_string(self):
        v = self.v
        kind = _enum(v["type"])
        if kind != "FOREIGN_KEY":
            return kind
        fk, pk = _columns(v["fk_columns"]), _columns(v["pk_columns"])
        info = v["info"]
        target = f'{_ident(info["schema"])}.{_ident(info["table"])}'
        if _enum(info["type"], "FK_TYPE_") == "PRIMARY_KEY_TABLE":
            return f"REFERENCED BY {target} ({fk}) -> ({pk})"
        return f"FOREIGN KEY ({fk}) REFERENCES {target} ({pk})"


# ---------- create / alter infos ---------------------------------------------

class CreateTableInfoPrinter:
    def __init__(self, v): self.v = v
    @_safe
    def to_string(self):
        schema = _deref(self.v["schema"])
        return f'CREATE TABLE {_ident(schema["name"])}.{_ident(self.v["table"])}'
    @_safe_children
    def children(self):
        yield "columns", self.v["columns"]
        for i, up in enumerate(_iter_std(self.v["constraints"])):
            yield f"constraint[{i}]", _pointee(up)
        yield "dependencies", self.v["dependencies"]


class FlatInfoPrinter:
    """Any CreateInfo / AlterInfo subclass: summary line + fields with bases flattened."""
    def __init__(self, v, verb):
        self.v = _dynamic(v)
        self.verb = verb

    @_safe
    def to_string(self):
        v = self.v
        target = ".".join(_text(x) for x in (_get(v, "schema"), _get(v, "name", "table")) if x is not None)
        s = f"{self.verb} {target}".strip()
        if _strip(v.type).tag == NS + "AlterForeignKeyInfo":
            s += (f'  {_enum(v["type"], "AFT_")} FK mirror <- {_text(v["fk_table"])}'
                  f' ({_columns(v["fk_columns"])}) -> ({_columns(v["pk_columns"])})')
        return s

    @_safe_children
    def children(self):
        yield from _flat_fields(self.v)


# ---------- registration -----------------------------------------------------

_EXACT = {
    "db7::Identifier": IdentifierPrinter,
    NS + "MangledEntryName": MangledNamePrinter,
    NS + "MangledDependencyName": MangledNamePrinter,
    NS + "CatalogEntryInfo": CatalogEntryInfoPrinter,
    NS + "LogicalDependency": LogicalDependencyPrinter,
    NS + "LogicalDependencyList": LogicalDependencyListPrinter,
    NS + "CatalogEntryMap": CatalogEntryMapPrinter,
    NS + "DependencyCatalogSet": DependencyCatalogSetPrinter,
    NS + "ForeignKeyInfo": ForeignKeyInfoPrinter,
    NS + "ColumnDefinition": ColumnDefinitionPrinter,
    NS + "ColumnList": ColumnListPrinter,
    NS + "CreateTableInfo": CreateTableInfoPrinter,
}


def _lookup(val):
    try:
        t = _strip(val.type)
        if t.code != gdb.TYPE_CODE_STRUCT or not t.tag or not t.tag.startswith("db7::"):
            return None
        v = _deref(val)
        if t.tag in _EXACT:
            return _EXACT[t.tag](v)
        if t.tag.startswith("db7::optional_ptr<"):
            return OptionalPtrPrinter(v)
        if _derives_from(t, NS + "Constraint"):
            return ConstraintPrinter(v)
        if _derives_from(t, NS + "CatalogEntry"):
            return CatalogEntryPrinter(v)
        if _derives_from(t, NS + "AlterInfo"):
            return FlatInfoPrinter(v, "ALTER")
        if _derives_from(t, NS + "CreateInfo"):
            return FlatInfoPrinter(v, "CREATE")
    except Exception:
        pass  # anything unexpected: fall back to GDB's raw view
    return None


# Replace a previously loaded version so re-sourcing works
gdb.pretty_printers[:] = [p for p in gdb.pretty_printers if getattr(p, "__name__", "") != "_lookup"]
gdb.pretty_printers.append(_lookup)