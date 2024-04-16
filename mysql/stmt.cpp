#include "stmt.h"

namespace coro
{
template <>
const enum_field_types E_FieldType<char>::type = MYSQL_TYPE_TINY;
template <>
const enum_field_types E_FieldType<int16_t>::type = MYSQL_TYPE_SHORT;
template <>
const enum_field_types E_FieldType<int32_t>::type = MYSQL_TYPE_LONG;
template <>
const enum_field_types E_FieldType<int64_t>::type = MYSQL_TYPE_LONGLONG;
template <>
const enum_field_types E_FieldType<float>::type = MYSQL_TYPE_FLOAT;
template <>
const enum_field_types E_FieldType<double>::type = MYSQL_TYPE_DOUBLE;
template <>
const enum_field_types E_FieldType<std::string>::type = MYSQL_TYPE_STRING;

void STMTBind::Add(std::string* data, size_t field_idx)
{
    MYSQL_BIND bind;
    bzero(&bind, sizeof(bind));
    bind.buffer_type = E_FieldType<std::string>::type;
    bind.buffer = &data->front();
    bind.buffer_length = data->size();
    bind.is_null = nullptr;
    m_size_vect[field_idx] = data->size();
    bind.length = &m_size_vect[field_idx];
    m_bind_vect[field_idx] = bind;
}

bool STMTBind::Bind(MYSQL_STMT* stmt)
{
    return mysql_stmt_bind_param(stmt, m_bind_vect.data()) == 0;
}

void STMTBind::Clear()
{
    m_bind_vect.clear();
    m_bind_vect.resize(m_field_count);

    m_size_vect.clear();
    m_size_vect.resize(m_field_count);
}

void STMTBind::SetFieldCount(size_t s)
{
    m_field_count = s;
    m_bind_vect.resize(s);
    m_size_vect.resize(s);
}

PreStatement::PreStatement(std::string table)
    : m_table(std::move(table))
{}

STMTField PreStatement::Add(const std::string& key)
{
    m_key_vect.emplace_back(key);
    return {m_key_vect.size() - 1, &m_bind};
}

void PreStatement::Init()
{
    m_bind.SetFieldCount(m_key_vect.size());
}

std::string PreStatement::Get()
{
    std::string buffer;
    std::vector<std::string> duplicate_update;
    for (auto& key : m_key_vect)
    {
        std::string value;
        fmt::format_to(std::back_inserter(value), "{} = VALUES({})", key, key);
        duplicate_update.emplace_back(std::move(value));
    }
    std::vector<std::string> value_vect(m_key_vect.size(), "?");
    fmt::format_to(std::back_inserter(buffer), "INSERT INTO {} ({}) VALUES({}) ON DUPLICATE KEY UPDATE {}", m_table, fmt::join(m_key_vect, ","), fmt::join(value_vect, ","), fmt::join(duplicate_update, ","));
    return buffer;
}

MYSQLStatement::MYSQLStatement(MYSQL* con)
{
    m_stmt = std::make_shared<STMT>(con);
}

bool MYSQLStatement::Init(const std::shared_ptr<PreStatement>& preparement)
{
    if (!m_stmt)
    {
        return false;
    }

    auto& stmt = m_stmt->m_stmt;
    auto sql = preparement->Get();
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0)
    {
        Trace(eum_LOG_WARN, "%s,\n 预处理错误 %s", sql.c_str(), mysql_stmt_error(m_stmt->m_stmt));
        return false;
    }
    m_preparement = preparement;
    m_preparement->Init();
    return true;
}

bool MYSQLStatement::Exec()
{
    bool ret = [&] {
        if (!m_preparement->m_bind.Bind(m_stmt->m_stmt))
        {
            Trace(eum_LOG_WARN, "MYSQL BIND执行错误 %s", mysql_stmt_error(m_stmt->m_stmt));
            return false;
        }

        if (mysql_stmt_execute(m_stmt->m_stmt))
        {
            Trace(eum_LOG_WARN, "MYSQL STMT执行错误 %s", mysql_stmt_error(m_stmt->m_stmt));
            return false;
        }
        return true;
    }();
    m_preparement->m_bind.Clear();
    return ret;
}

}  // namespace coro