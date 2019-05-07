#ifndef _VM_H
#define _VM_H
#include "disk_manager.h"
#include "buffer_pool.h"
#include "BplusTree.h"
#include "thread_pool.h"
#include "page.h"
#include "table.h"
#include "query.h"
#include <future>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <memory>
#include <deque>
#include <optional>

namespace DB::tree { class BTree; }
namespace DB::ast {
    struct BaseExpr;
    struct BaseOp;
    struct ProjectOp;
    struct FilterOp;
    struct JoinOp;
    struct TableOp;
}

namespace DB::vm
{

    using table::table_view;

    struct StorageEngine
    {
        disk::DiskManager* disk_manager_;
        buffer::BufferPoolManager* buffer_pool_manager_;
    };


    class ConsoleReader
    {
        ConsoleReader(const ConsoleReader&) = delete;
        ConsoleReader(ConsoleReader&&) = delete;
        ConsoleReader& operator=(const ConsoleReader&) = delete;
        ConsoleReader& operator=(ConsoleReader&&) = delete;
    public:
        ConsoleReader() = default;
        void start();
        void stop();
        std::string get_sql(); // might stuck
        void add_sql(std::string); // only used when redo
    private:
        std::queue<std::string> sql_pool_;
        std::thread reader_;
        std::mutex sql_pool_mutex_;
        std::condition_variable sql_pool_cv_;
    };


    using table::VirtualTable;
    using table::row_view;
    //
    //
    //
    //
    class VM
    {
        friend class disk::DiskManager;
    public:

        VM();
        ~VM();

        VM(const VM&) = delete;
        VM(VM&&) = delete;
        VM& operator=(const VM&) = delete;
        VM& operator=(VM&&) = delete;

        void init();

        // run db task until user input "exit"
        void start();

        template<typename F, typename... Args>[[nodiscard]]
            std::future<std::invoke_result_t<F, Args...>> register_task(F&& f, Args&& ...args) {
            return task_pool_.register_for_execution(f, args...);
        }

        void set_next_free_page_id(page::page_id_t);

        std::optional<table::TableInfo> getTableInfo(const std::string& tableName);

    private:

        void send_reply_sql(std::string);


        struct process_result_t {
            bool exit = false;
            bool error = false;
            std::string msg;
        };
        process_result_t query_process(const query::SQLValue&);

        void doWAL(page::page_id_t prev_last_page_id, const std::string& sql);

        // flush dirty page into disk.
        void flush();

        void detroy_log();

        // return   OK   : no need to reply log.
        //          UNDO : undo the last txn.
        //          REDO : undo the last txn, then redo.
        disk::log_state_t check_log();
        void replay_log(disk::log_state_t);


        // query process functions
        void doCreate(process_result_t&, const query::CreateTableInfo&);
        void doDrop(process_result_t&, const query::DropTableInfo&);
        void doSelect(process_result_t&, const query::SelectInfo&);
        void doUpdate(process_result_t&, const query::UpdateInfo&);
        void doInsert(process_result_t&, const query::InsertInfo&);
        void doDelete(process_result_t&, const query::DeleteInfo&);




        // 4 kinds of op node
        // - scanTable
        // - join
        // - projection
        // - sigma
    public:
        //friend struct TableOp;
        VirtualTable scanTable(const std::string& tableName);
        void doScanTable(VirtualTable ret, const std::string tableName);

        //friend struct JoinOp;
        VirtualTable join(VirtualTable t1, VirtualTable t2, bool pk);
        void doJoin(VirtualTable ret, VirtualTable t1, VirtualTable t2, bool pk);

        //friend struct ProjectOp;
        VirtualTable projection(VirtualTable t, const std::vector<std::string>& colNames);
        void doProjection(VirtualTable ret, VirtualTable t);

        //friend struct FilterOp;
        VirtualTable sigma(VirtualTable t, ast::BaseExpr*);
        void doSigma(VirtualTable ret, VirtualTable t, ast::BaseExpr*);


        void init_pk_view();

    private:

        StorageEngine storage_engine_;
        util::ThreadsPool task_pool_;
        ConsoleReader conslole_reader_;
        page::DBMetaPage* db_meta_;
        std::unordered_map<std::string, page::TableMetaPage*> table_meta_;
        std::unordered_map<std::string, table::TableInfo> table_info_;
        std::unordered_map<std::string, page::TableMetaPage*> free_table_;


        // PK view: pk -> ref
        // if the PK is not ref by any FK, the ref = 1;
        friend struct table::TableInfo;
        std::unordered_map<page::page_id_t,
            std::unordered_map<int32_t, uint32_t>> table_pk_ref_INT;
        std::unordered_map<page::page_id_t,
            std::unordered_map<std::string, uint32_t>> table_pk_ref_VARCHAR;


    public: // for test
        void test_create_table();
        uint32_t test_insert(const tree::KVEntry&);
        uint32_t test_erase(const page::KeyEntry&);
        page::ValueEntry test_find(const page::KeyEntry&);
        void test_size();
        void test_output();
        void test_flush();
        void showDB();

        template<typename ...Arg>
        void printXJBDB(const char* format, Arg... args) {
            std::printf("xjbDB$$$$$: ");
            std::printf(format, args...);
        }

        void println();

    };



} // end namespace DB::vm

#endif // !_VM_H