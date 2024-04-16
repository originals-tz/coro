#ifndef CORO_MYSQL_STMT_H
#define CORO_MYSQL_STMT_H

#include <field_types.h>
#include <fmt/format.h>
#include <frame/net/trace.h>
#include <mysql.h>
#include <any>
#include <cstring>
#include <functional>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace coro
{
template <typename T>
struct E_FieldType
{
    //! 对应的mysql字段类型
    static const enum_field_types type;
};

class STMTBind
{
public:
    /**
     * @brief 添加mysql字段
     * @tparam T 类型
     * @param data 数据
     * @param field_idx 所在位置
     */
    template <typename T>
    void Add(T* data, size_t field_idx)
    {
        MYSQL_BIND bind;
        bzero(&bind, sizeof(bind));
        bind.buffer_type = E_FieldType<T>::type;
        bind.buffer = data;
        bind.is_null = nullptr;
        bind.length = nullptr;
        m_bind_vect[field_idx] = bind;
    }
    /**
     * @brief 添加mysql字段
     * @param data 数据
     * @param field_idx 所在位置
     */
    void Add(std::string* data, size_t field_idx);

    /**
     * @brief 绑定stmt
     * @param stmt
     * @return
     */
    bool Bind(MYSQL_STMT* stmt);

    /**
     * @brief 清空数据
     */
    void Clear();

    /**
     * @brief 设置字段数量
     * @param s
     */
    void SetFieldCount(size_t s);

private:
    //! 字段数量
    size_t m_field_count = 0;
    //! mysql字段绑定
    std::vector<MYSQL_BIND> m_bind_vect;
    //! 字段长度
    std::vector<size_t> m_size_vect;

};

class STMTField
{
public:
    STMTField(size_t idx, STMTBind* bind)
        : m_idx(idx)
        , m_bind(bind)
    {}

    /**
     * @brief 设置数据
     * @tparam T
     * @param data
     * @return
     */
    template <typename T>
    bool Set(T* data)
    {
        m_bind->Add(data, m_idx);
        return true;
    }

private:
    //! 容器，存放const数据
    std::any m_tmp;
    //! 对应的索引
    size_t m_idx = 0;
    //! bind
    STMTBind* m_bind = nullptr;
};

class MYSQLStatement;
class PreStatement
{
public:
    friend class MYSQLStatement;
    explicit PreStatement(std::string table);

    /**
     * @brief 添加一个字段
     * @param key key
     * @return 字段
     */
    STMTField Add(const std::string& key);

    /**
     * @brief 初始化
     */
    void Init();

    /**
     * @brief 获取sql
     * @return
     */
    std::string Get();

private:
    //! 字段绑定
    STMTBind m_bind;
    //! 数据库表
    std::string m_table;
    //! 需要设置的字段
    std::vector<std::string> m_key_vect;
};

struct STMT
{
    explicit STMT(MYSQL* con) { m_stmt = mysql_stmt_init(con); }
    ~STMT()
    {
        if (m_stmt)
        {
            mysql_stmt_close(m_stmt);
        }
    }
    MYSQL_STMT* m_stmt;
};

class MYSQLStatement
{
public:
    explicit MYSQLStatement(MYSQL* con);

    /**
     * @brief 初始化预处理
     * @param preparement
     * @return
     */
    bool Init(const std::shared_ptr<PreStatement>& preparement);

    /**
     * @brief 执行插入
     * @return
     */
    bool Exec();

private:
    //! mysql stmt
    std::shared_ptr<STMT> m_stmt;
    //! 预处理
    std::shared_ptr<PreStatement> m_preparement;
};

}  // namespace coro

#endif  // CORO_MYSQL_STMT_H
