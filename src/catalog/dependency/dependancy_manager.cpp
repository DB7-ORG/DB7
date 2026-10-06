#include "catalog/catalog_common.hpp"
#include "catalog/catalog_entry_helper.hpp"
#include "catalog/database_catalog.hpp"
#include "catalog/dependency/dependency_catalog_set.hpp"
#include "catalog/dependency/dependency_manager.hpp"
#include "catalog/entries/dependency_dependent_entry.hpp"
#include "catalog/entries/dependency_entry.hpp"
#include "catalog/entries/dependency_subject_entry.hpp"
#include "catalog/entries/schema_catalog_entry_base.hpp"

namespace db7::catalog {
DependencyManager::DependencyManager(DatabaseCatalog &catalog)
    : catalog(catalog), subjects(catalog), dependents(catalog) {}

static void AssertMangledName(const std::string &mangled_name, idx_t expected_null_bytes) {
#ifdef DB7DEBUG
  idx_t nullbyte_count = 0;
  for (auto &ch : mangled_name) { nullbyte_count += ch == '\0'; }
  DB7_ASSERT(nullbyte_count == expected_null_bytes, "invalid number of null chars");
#endif
}

MangledEntryName::MangledEntryName(const CatalogEntryInfo &info) {
  auto &type = info.type;
  auto &schema = info.schema;
  auto &name = info.name;

  this->name = Identifier(CatalogTypeToString(type) + '\0' + schema + '\0' + name);
  AssertMangledName(this->name.GetIdentifierName(), 2);
}

MangledDependencyName::MangledDependencyName(const MangledEntryName &from,
                                             const MangledEntryName &to) {
  this->name = Identifier(from.name + '\0' + to.name);
  AssertMangledName(this->name.GetIdentifierName(), 5);
}

Identifier DependencyManager::GetSchema(const CatalogEntry &entry) {
  if (entry.type == CatalogType::SCHEMA_ENTRY) { return entry.name; }
  return entry.ParentSchema().name;
}

CatalogEntryInfo DependencyManager::GetLookupProperties(const CatalogEntry &entry) {
  if (entry.type == CatalogType::DEPENDENCY_ENTRY) {
    auto &dependency_entry = entry.Cast<DependencyEntry>();
    return dependency_entry.EntryInfo();
  } else {
    auto schema = DependencyManager::GetSchema(entry);
    auto &name = entry.name;
    auto &type = entry.type;
    return CatalogEntryInfo{type, Identifier(schema), name};
  }
}

optional_ptr<CatalogEntry> DependencyManager::LookupEntry(transaction::TransactionContext &context,
                                                          CatalogEntry &dependency) {
  // ignore if its not a dependency
  if (dependency.type != CatalogType::DEPENDENCY_ENTRY) { return &dependency; }

  auto info = GetLookupProperties(dependency);

  return LookupEntry(context, info);
}

optional_ptr<CatalogEntry> DependencyManager::LookupEntry(transaction::TransactionContext &context,
                                                          const CatalogEntryInfo &info) {

  auto &type = info.type;
  auto &schema = info.schema;
  auto &name = info.name;

  // Lookup the schema
  auto schema_entry = catalog.GetSchema(context, schema, OnEntryNotFound::RETURN_NULL);
  if (type == CatalogType::SCHEMA_ENTRY || !schema_entry) {
    // This is a schema entry, perform the callback only providing the schema
    return reinterpret_cast<CatalogEntry *>(schema_entry.get());
  }
  auto entry = schema_entry->GetEntry(context, type, name);
  return entry;
}

CatalogSet &DependencyManager::Dependents() { return dependents; }

CatalogSet &DependencyManager::Subjects() { return subjects; }

MangledEntryName DependencyManager::MangleName(const CatalogEntryInfo &info) {
  return MangledEntryName(info);
}

void DependencyManager::ScanSetInternal(transaction::TransactionContext &context,
                                        const CatalogEntryInfo &info, bool scan_subjects,
                                        dependency_callback_t &callback) {
  catalog_entry_set_t other_entries;

  auto cb = [&](CatalogEntry &other) {
    DB7_ASSERT(other.type == CatalogType::DEPENDENCY_ENTRY, "");
    auto &other_entry = other.Cast<DependencyEntry>();
#ifdef DB7DEBUG
    auto side = other_entry.Side();
    if (scan_subjects) {
      DB7_ASSERT(side == DependencyEntryType::SUBJECT);
    } else {
      DB7_ASSERT(side == DependencyEntryType::DEPENDENT);
    }

#endif

    other_entries.insert(other_entry);
    callback(other_entry);
  };

  if (scan_subjects) {
    DependencyCatalogSet subjects(Subjects(), info);
    subjects.Scan(context, cb);
  } else {
    DependencyCatalogSet dependents(Dependents(), info);
    dependents.Scan(context, cb);
  }

#ifdef DB7DEBUG
  // Verify some invariants
  // Every dependency should have a matching dependent in the other set
  // And vice versa
  auto mangled_name = MangleName(info);

  if (scan_subjects) {
    for (auto &entry : other_entries) {
      auto other_info = GetLookupProperties(entry);
      DependencyCatalogSet other_dependents(Dependents(), other_info);

      // Verify that the other half of the dependency also exists
      auto dependent = other_dependents.GetEntryDetailed(context, mangled_name);
      DB7_ASSERT(dependent.reason != CatalogSet::EntryLookup::FailureReason::NOT_PRESENT);
    }
  } else {
    for (auto &entry : other_entries) {
      auto other_info = GetLookupProperties(entry);
      DependencyCatalogSet other_subjects(Subjects(), other_info);

      // Verify that the other half of the dependent also exists
      auto subject = other_subjects.GetEntryDetailed(context, mangled_name);
      DB7_ASSERT(subject.reason != CatalogSet::EntryLookup::FailureReason::NOT_PRESENT);
    }
  }
#endif
}

void DependencyManager::ScanDependents(transaction::TransactionContext &context,
                                       const CatalogEntryInfo &info,
                                       dependency_callback_t &callback) {
  ScanSetInternal(context, info, false, callback);
}

void DependencyManager::ScanSubjects(transaction::TransactionContext &context,
                                     const CatalogEntryInfo &info,
                                     dependency_callback_t &callback) {
  ScanSetInternal(context, info, true, callback);
}

// void DependencyManager::Scan(
//     transaction::TransactionContext &context,
//     const std::function<void(CatalogEntry &, CatalogEntry &, const DependencyDependentFlags &)>
//         &callback) {
//   std::lock_guard<std::mutex> write_lock(catalog.GetLock());

//   // // All the objects registered in the dependency manager
//   catalog_entry_set_t entries;
//   dependents.Scan(context, [&](CatalogEntry &set) {
//     auto entry = LookupEntry(context, set);
//     entries.insert(*entry);
//   });

//   // // For every registered entry, get the dependents
//   for (auto &entry : entries) {
//     auto entry_info = GetLookupProperties(entry);
//     // Scan all the dependents of the entry
//     ScanDependents(context, entry_info, [&](DependencyEntry &dependent) {
//       auto dep = LookupEntry(context, dependent);
//       if (!dep) { return; }
//       auto &dependent_entry = *dep;
//       callback(entry, dependent_entry, dependent.Dependent().flags);
//     });
//   }
// }

// NOTE: this was modified compared to duck db original code
void DependencyManager::Scan(
    transaction::TransactionContext &context,
    const std::function<void(CatalogEntry &, CatalogEntry &, const DependencyDependentFlags &)>
        &callback) {
  std::lock_guard<std::mutex> write_lock(catalog.GetLock());

  // Every entry in the dependents set is one edge: subject <- dependent
  dependents.Scan(context, [&](CatalogEntry &e) {
    auto &edge = e.Cast<DependencyEntry>();
    auto subject = LookupEntry(context, edge.Subject().entry);
    auto dependent = LookupEntry(context, edge.Dependent().entry);
    if (!subject || !dependent) { return; }
    if (subject->type == CatalogType::SCHEMA_ENTRY) { return; }
    callback(*subject, *dependent, edge.Dependent().flags);
  });
}

bool DependencyManager::IsSystemEntry(CatalogEntry &entry) const {
  if (entry.internal) { return true; }

  switch (entry.type) {
  case CatalogType::DEPENDENCY_ENTRY:
  case CatalogType::DATABASE_ENTRY:
  case CatalogType::RENAMED_ENTRY: return true;
  default: return false;
  }
}

void DependencyManager::CreateSubject(transaction::TransactionContext &context,
                                      const DependencyInfo &info) {
  auto &from = info.dependent.entry;

  DependencyCatalogSet set(Subjects(), from);
  std::unique_ptr<DependencyEntry> dep = std::make_unique<DependencySubjectEntry>(catalog, info);
  auto entry_name = dep->EntryMangledName();

  //! Add to the list of objects that 'dependent' has a dependency on
  set.CreateEntry(context, entry_name, std::move(dep));
}

void DependencyManager::CreateDependent(transaction::TransactionContext &context,
                                        const DependencyInfo &info) {
  auto &from = info.subject.entry;

  DependencyCatalogSet set(Dependents(), from);
  std::unique_ptr<DependencyEntry> dep = std::make_unique<DependencyDependentEntry>(catalog, info);
  auto entry_name = dep->EntryMangledName();

  //! Add to the list of object that depend on 'subject'
  set.CreateEntry(context, entry_name, std::move(dep));
}

void DependencyManager::CreateDependency(transaction::TransactionContext &context,
                                         DependencyInfo &info) {
  DependencyCatalogSet subjects(Subjects(), info.dependent.entry);
  DependencyCatalogSet dependents(Dependents(), info.subject.entry);

  auto subject_mangled = MangleName(info.subject.entry);
  auto dependent_mangled = MangleName(info.dependent.entry);

  auto &dependent_flags = info.dependent.flags;
  auto &subject_flags = info.subject.flags;

  auto existing_subject = subjects.GetEntry(context, subject_mangled);
  auto existing_dependent = dependents.GetEntry(context, dependent_mangled);

  // Inherit the existing flags and drop the existing entry if present
  if (existing_subject) {
    auto &existing = existing_subject->Cast<DependencyEntry>();
    auto existing_flags = existing.Subject().flags;
    if (existing_flags != subject_flags) { subject_flags.Apply(existing_flags); }
    subjects.DropEntry(context, subject_mangled, false, false);
  }
  if (existing_dependent) {
    auto &existing = existing_dependent->Cast<DependencyEntry>();
    auto existing_flags = existing.Dependent().flags;
    if (existing_flags != dependent_flags) { dependent_flags.Apply(existing_flags); }
    dependents.DropEntry(context, dependent_mangled, false, false);
  }

  // Create an entry in the dependents map of the object that is the target of the dependency
  CreateDependent(context, info);
  // Create an entry in the subjects map of the object that is targeting another entry
  CreateSubject(context, info);
}

void DependencyManager::CreateDependencies(transaction::TransactionContext &context,
                                           const CatalogEntry &object,
                                           const LogicalDependencyList &dependencies) {
  DependencyDependentFlags dependency_flags;
  if (object.type != CatalogType::INDEX_ENTRY) {
    // indexes do not require CASCADE to be dropped, they are simply always dropped along with the
    // table
    dependency_flags.SetBlocking();
  }

  const auto object_info = GetLookupProperties(object);
  // check for each object in the sources if they were not deleted yet
  for (auto &dependency : dependencies.Set()) {
    if (dependency.catalog != object.ParentCatalog().GetName()) {
      throw CATALOG_EXCEPTION(fmt::format(
          "Error adding dependency for object {} - dependency {} is in catalog "
          "{}, which does not match the catalog {}.\nCross catalog dependencies are not "
          "supported.",
          object.name.GetIdentifierName(), dependency.entry.name.GetIdentifierName(),
          dependency.catalog.GetIdentifierName(),
          object.ParentCatalog().GetName().GetIdentifierName()));
    }
  }

  // add the object to the dependents_map of each object that it depends on
  for (auto &dependency : dependencies.Set()) {
    DependencyInfo info{
        /*dependent = */ DependencyDependent{GetLookupProperties(object), dependency_flags},
        /*subject = */ DependencySubject{dependency.entry, DependencySubjectFlags()}};
    CreateDependency(context, info);
  }
}

void DependencyManager::AddObject(transaction::TransactionContext &context, CatalogEntry &object,
                                  const LogicalDependencyList &dependencies) {
  if (IsSystemEntry(object)) {
    // Don't do anything for this
    return;
  }
  CreateDependencies(context, object, dependencies);
}

static bool CascadeDrop(bool cascade, const DependencyDependentFlags &flags) {
  if (cascade) { return true; }
  if (flags.IsOwnedBy()) {
    // We are owned by this object, while it exists we can not be dropped without cascade.
    return false;
  }
  return !flags.IsBlocking();
}

static std::string EntryToString(CatalogEntryInfo &info) {
  auto type = info.type;
  switch (type) {
  case CatalogType::TABLE_ENTRY: {
    return fmt::format("table {}", info.name.GetIdentifierName());
  }
  case CatalogType::SCHEMA_ENTRY: {
    return fmt::format("schema {}", info.name.GetIdentifierName());
  }
  case CatalogType::VIEW_ENTRY: {
    return fmt::format("view {}", info.name.GetIdentifierName());
  }
  case CatalogType::INDEX_ENTRY: {
    return fmt::format("index {}", info.name.GetIdentifierName());
  }
  case CatalogType::SEQUENCE_ENTRY: {
    return fmt::format("sequence {}", info.name.GetIdentifierName());
  }
  case CatalogType::COLLATION_ENTRY: {
    return fmt::format("collation {}", info.name.GetIdentifierName());
  }
  case CatalogType::COORDINATE_SYSTEM_ENTRY: {
    return fmt::format("coordinate system {}", info.name.GetIdentifierName());
  }
  case CatalogType::TYPE_ENTRY: {
    return fmt::format("type {}", info.name.GetIdentifierName());
  }
  case CatalogType::TABLE_FUNCTION_ENTRY: {
    return fmt::format("table function {}", info.name.GetIdentifierName());
  }
  case CatalogType::SCALAR_FUNCTION_ENTRY: {
    return fmt::format("scalar function {}", info.name.GetIdentifierName());
  }
  case CatalogType::AGGREGATE_FUNCTION_ENTRY: {
    return fmt::format("aggregate function {}", info.name.GetIdentifierName());
  }
  case CatalogType::PRAGMA_FUNCTION_ENTRY: {
    return fmt::format("pragma function {}", info.name.GetIdentifierName());
  }
  case CatalogType::COPY_FUNCTION_ENTRY: {
    return fmt::format("copy function {}", info.name.GetIdentifierName());
  }
  case CatalogType::MACRO_ENTRY: {
    return fmt::format("macro function {}", info.name.GetIdentifierName());
  }
  case CatalogType::TABLE_MACRO_ENTRY: {
    return fmt::format("table macro function {}", info.name.GetIdentifierName());
  }
  case CatalogType::SECRET_ENTRY: {
    return fmt::format("secret {}", info.name.GetIdentifierName());
  }
  case CatalogType::SECRET_TYPE_ENTRY: {
    return fmt::format("secret type {}", info.name.GetIdentifierName());
  }
  case CatalogType::SECRET_FUNCTION_ENTRY: {
    return fmt::format("secret function {}", info.name.GetIdentifierName());
  }
  case CatalogType::TRIGGER_ENTRY: {
    return fmt::format("trigger {}", info.name.GetIdentifierName());
  }
  default: throw;
  };
}
std::string DependencyManager::CollectDependents(transaction::TransactionContext &context,
                                                 catalog_entry_set_t &entries,
                                                 CatalogEntryInfo &info) {
  std::string result;
  for (auto &entry : entries) {
    DB7_ASSERT(!IsSystemEntry(entry.get()), "");
    auto other_info = GetLookupProperties(entry);
    result += fmt::format("{} depends on {}.\n", EntryToString(other_info), EntryToString(info));
    catalog_entry_set_t entry_dependents;
    ScanDependents(context, other_info, [&](DependencyEntry &dep) {
      auto child = LookupEntry(context, dep);
      if (!child) { return; }
      if (!CascadeDrop(false, dep.Dependent().flags)) { entry_dependents.insert(*child); }
    });
    if (!entry_dependents.empty()) {
      result += CollectDependents(context, entry_dependents, other_info);
    }
  }
  return result;
}

catalog_entry_set_t
DependencyManager::CheckDropDependencies(transaction::TransactionContext &context,
                                         CatalogEntry &object, bool cascade) {
  if (IsSystemEntry(object)) {
    // Don't do anything for this
    return catalog_entry_set_t();
  }

  catalog_entry_set_t to_drop;
  catalog_entry_set_t blocking_dependents;

  auto info = GetLookupProperties(object);
  // Look through all the objects that depend on the 'object'
  ScanDependents(context, info, [&](DependencyEntry &dep) {
    // It makes no sense to have a schema depend on anything
    DB7_ASSERT(dep.EntryInfo().type != CatalogType::SCHEMA_ENTRY, "");
    auto entry = LookupEntry(context, dep);
    if (!entry) { return; }

    if (!CascadeDrop(cascade, dep.Dependent().flags)) {
      // no cascade and there are objects that depend on this object: throw error
      blocking_dependents.insert(*entry);
    } else {
      to_drop.insert(*entry);
    }
  });
  if (!blocking_dependents.empty()) {
    std::string error_string =
        fmt::format("Cannot drop entry {} because there are entries that depend on it.\n",
                    object.name.GetIdentifierName());
    error_string += CollectDependents(context, blocking_dependents, info);
    error_string += "Use DROP...CASCADE to drop all dependents.";
    throw CATALOG_EXCEPTION(error_string);
  }

  // Look through all the entries that 'object' depends on
  ScanSubjects(context, info, [&](DependencyEntry &dep) {
    auto flags = dep.Subject().flags;
    if (flags.IsOwnership()) {
      // We own this object, it should be dropped along with the table
      auto entry = LookupEntry(context, dep);
      to_drop.insert(*entry);
    }
  });
  return to_drop;
}

void DependencyManager::RemoveDependency(transaction::TransactionContext &context,
                                         const DependencyInfo &info) {
  auto &dependent = info.dependent;
  auto &subject = info.subject;

  // The dependents of the dependency (target)
  DependencyCatalogSet dependents(Dependents(), subject.entry);
  // The subjects of the dependencies of the dependent
  DependencyCatalogSet subjects(Subjects(), dependent.entry);

  auto dependent_mangled = MangledEntryName(dependent.entry);
  auto subject_mangled = MangledEntryName(subject.entry);

  auto dependent_p = dependents.GetEntry(context, dependent_mangled);
  if (dependent_p) {
    // 'dependent' is no longer inhibiting the deletion of 'dependency'
    dependents.DropEntry(context, dependent_mangled, false);
  }
  auto subject_p = subjects.GetEntry(context, subject_mangled);
  if (subject_p) {
    // 'dependency' is no longer required by 'dependent'
    subjects.DropEntry(context, subject_mangled, false);
  }
}

void DependencyManager::CleanupDependencies(transaction::TransactionContext &context,
                                            CatalogEntry &object) {
  // Collect the dependencies
  std::vector<DependencyInfo> to_remove;

  auto info = GetLookupProperties(object);
  ScanSubjects(context, info, [&](DependencyEntry &dep) {
    to_remove.push_back(DependencyInfo::FromSubject(dep));
  });
  ScanDependents(context, info, [&](DependencyEntry &dep) {
    to_remove.push_back(DependencyInfo::FromDependent(dep));
  });

  // Remove the dependency entries
  for (auto &dep : to_remove) { RemoveDependency(context, dep); }
}

void DependencyManager::DropObject(transaction::TransactionContext &context, CatalogEntry &object,
                                   bool cascade) {
  if (IsSystemEntry(object)) {
    // Don't do anything for this
    return;
  }

  // Check if there are any entries that block the DROP because they still depend on the object
  auto to_drop = CheckDropDependencies(context, object, cascade);
  CleanupDependencies(context, object);

  for (auto &entry : to_drop) {
    auto set = entry.get().set;
    DB7_ASSERT(set, "");
    set->DropEntry(context, entry.get().name, cascade);
  }
}

DependencyInfo DependencyInfo::FromSubject(DependencyEntry &dep) {
  return DependencyInfo{/*dependent = */ dep.Dependent(),
                        /*subject = */ dep.Subject()};
}

DependencyInfo DependencyInfo::FromDependent(DependencyEntry &dep) {
  return DependencyInfo{/*dependent = */ dep.Dependent(),
                        /*subject = */ dep.Subject()};
}

void DependencyManager::AlterObject(transaction::TransactionContext &context, CatalogEntry &old_obj,
                                    CatalogEntry &new_obj, AlterInfo &info) {
  // Dont change anything for system entries
  if (IsSystemEntry(new_obj)) {
    DB7_ASSERT(IsSystemEntry(old_obj));
    // Don't do anything for this
    return;
  }

  const auto old_info = GetLookupProperties(old_obj);
  const auto new_info = GetLookupProperties(new_obj);

  std::vector<DependencyInfo> dependencies;
  // Other entries that depend on us
  ScanDependents(context, old_info, [&](DependencyEntry &dep) {
    // It makes no sense to have a schema depend on anything
    DB7_ASSERT(dep.EntryInfo().type != CatalogType::SCHEMA_ENTRY);

    bool disallow_alter = true;
    switch (info.type) {
    case AlterType::ALTER_TABLE: {
      auto &alter_table = info.Cast<AlterTableInfo>();
      switch (alter_table.alter_table_type) {
      case AlterTableType::FOREIGN_KEY_CONSTRAINT:
      case AlterTableType::ADD_COLUMN:
      case AlterTableType::SET_DEFAULT: {
        disallow_alter = false;
        break;
      }
      default: break;
      }
      break;
    }
    case AlterType::SET_COLUMN_COMMENT:
    case AlterType::SET_COMMENT: {
      disallow_alter = false;
      break;
    }
    default: break;
    }
    if (disallow_alter) {
      throw CATALOG_EXCEPTION(fmt::format("Cannot alter entry {} because there are entries that "
                                          "depend on it.",
                                          old_obj.name.GetIdentifierName()));
    }

    auto dep_info = DependencyInfo::FromDependent(dep);
    dep_info.subject.entry = new_info;
    dependencies.emplace_back(dep_info);
  });

  // Keep old dependencies
  bool has_new_dependencies = info.new_dependencies.get();
  ScanSubjects(context, old_info, [&](DependencyEntry &dep) {
    if (has_new_dependencies && !dep.Subject().flags.IsOwnership()) {
      // The alter provided updated dependencies - skip old non-ownership subject dependencies
      // as they will be replaced by the new dependencies
      return;
    }
    auto entry = LookupEntry(context, dep);
    if (!entry) { return; }

    auto dep_info = DependencyInfo::FromSubject(dep);
    dep_info.dependent.entry = new_info;
    dependencies.emplace_back(dep_info);
  });

  if (has_new_dependencies || !(old_obj.name == new_obj.name)) {
    // The dependencies have changed (e.g. SET DEFAULT) or the name has changed
    // We need to recreate the dependency links
    CleanupDependencies(context, old_obj);
  }

  if (has_new_dependencies) {
    // Add the new dependencies
    CreateDependencies(context, new_obj, *info.new_dependencies);
  }

  // Reinstate any old dependencies
  for (auto &dep : dependencies) { CreateDependency(context, dep); }
}

} // namespace db7::catalog