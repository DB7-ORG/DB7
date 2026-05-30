import gdb  # type: ignore
import json
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer


def gdb_eval(expr):
    """Must be called from GDB's main thread — use gdb.post_event for this."""
    return gdb.parse_and_eval(expr)


def atomic_val(node):
    """Read the underlying value of a std::atomic from a gdb.Value."""
    # libstdc++: _M_i  |  libc++: __a_ -> __val_
    for field in ("_M_i", "__a_"):
        try:
            return node[field]
        except gdb.error:
            pass
    return node  # fallback: hope for the best


def getPoolSize(result):
    result["pool_size"] = int(gdb_eval("buffer_pool")["pool_size_"])


def getPagesByTableId(result, table_id):
    pages = gdb_eval("buffer_pool.pages_")
    size = int(
        atomic_val(gdb_eval("buffer_pool")["pool_size_"])
    )  # if pool_size_ is also atomic
    arr = []

    for i in range(0, size):
        page = pages[i]
        page_id_val = atomic_val(page["id_"])  # std::atomic<PageIdentifier>
        tbl_id = int(page_id_val["tbl_id"])
        pid = int(page_id_val["pid"])

        if table_id != tbl_id:
            continue

        ref_count = int(atomic_val(page["ref_count_"]))  # std::atomic<u32>
        flags = int(atomic_val(page["flags_"]))  # std::atomic<u8>

        arr.append(
            {
                "index": i,
                "tbl_id": tbl_id,
                "pid": pid,
                "pin_count": ref_count,
                "is_dirty": bool(flags & 0x1),  # adjust mask to your kDirtyFlag
                "io_in_progress": bool(flags & 0x2),  # adjust mask to your kIOFlag
                "frame_ptr": str(page["data_"]),  # raw ptr, not atomic
            }
        )

    arr.sort(key=lambda p: p["pid"])
    result["pages"] = arr


def getTablesById(result):
    pages = gdb_eval("buffer_pool.pages_")
    size = int(atomic_val(gdb_eval("buffer_pool")["pool_size_"]))
    seen = set()
    tables = []
    for i in range(0, size):
        page = pages[i]
        page_id_val = atomic_val(page["id_"])
        tbl_id = int(page_id_val["tbl_id"])
        if tbl_id not in seen:
            seen.add(tbl_id)
            tables.append({"table_id": tbl_id, "name": "table_" + str(tbl_id)})
    result["tables"] = tables


def getPageById(result, index):
    page = gdb_eval(f"buffer_pool.pages_[{index}]")
    data_ptr = int(page["data_"])

    dump = gdb.parse_and_eval(f"InspectVarlenLayout((byte *){data_ptr})")

    # dump is now a pointer — dereference it
    count = int(dump["count"])
    result["page"] = {
        "rlink": int(dump["rlink"]),
        "count": count,
        "level": int(dump["level"]),
        "max_val": int(dump["max_val"]),
        "max_val_key": dump["max_val_key"].string(),
        "slots": [
            {
                "slot": int(dump["slots"][i]["slot"]),
                "offset": int(dump["slots"][i]["offset"]),
                "len": int(dump["slots"][i]["len"]),
                "result": int(dump["slots"][i]["result_val"]),
                "key": dump["slots"][i]["key"].string(),
            }
            for i in range(count)
        ],
    }


class Handler(BaseHTTPRequestHandler):

    def eval_in_gdb(self, result, done):
        try:
            if self.path == "/pool/size":
                getPoolSize(result)
            elif self.path.startswith("/pool/page/"):
                index = int(self.path.split("/")[-1])
                getPageById(result, index)
            elif self.path.startswith("/pool/pages/"):
                table_id = int(self.path.split("/")[-1])
                getPagesByTableId(result, table_id)
            elif self.path == "/pool/tables":
                getTablesById(result)
            else:
                self.send_response(404)
                self.end_headers()
                return
        except gdb.error as e:
            result["error"] = str(e)
        finally:
            done.set()

    def do_GET(self):

        print(f"[inspector] HTTP server {self.path}")

        result = {}

        # GDB eval must happen on GDB's main thread
        done = threading.Event()

        gdb.post_event(
            lambda: self.eval_in_gdb(result, done)
        )  # schedule on GDB main thread
        done.wait(timeout=5.0)  # wait for result

        # CORS so browser can call directly
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(result).encode())

    def log_message(self, format, *args):
        pass  # silence request logs


class ReusableHTTPServer(HTTPServer):
    allow_reuse_address = True


def start_server():
    try:
        server = ReusableHTTPServer(("127.0.0.1", 8007), Handler)
        print("[inspector] HTTP server on http://127.0.0.1:8007")
        server.serve_forever()
    except Exception as e:
        print(f"[inspector] FAILED to start server: {e}")


# Start server in background thread when script loads
threading.Thread(target=start_server, daemon=True).start()
