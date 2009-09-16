#include <dbus/dbus.h>
#include <map>
#include <vector>
#include "abrtlib.h"

extern DBusConnection* g_dbus_conn;

/*
 * Helpers for building DBus messages
 */

//void store_bool(DBusMessageIter* iter, bool val);
void store_int32(DBusMessageIter* iter, int32_t val);
void store_uint32(DBusMessageIter* iter, uint32_t val);
void store_int64(DBusMessageIter* iter, int64_t val);
void store_uint64(DBusMessageIter* iter, uint64_t val);
void store_string(DBusMessageIter* iter, const char* val);

//static inline void store_val(DBusMessageIter* iter, bool val)               { store_bool(iter, val); }
static inline void store_val(DBusMessageIter* iter, int32_t val)            { store_int32(iter, val); }
static inline void store_val(DBusMessageIter* iter, uint32_t val)           { store_uint32(iter, val); }
static inline void store_val(DBusMessageIter* iter, int64_t val)            { store_int64(iter, val); }
static inline void store_val(DBusMessageIter* iter, uint64_t val)           { store_uint64(iter, val); }
static inline void store_val(DBusMessageIter* iter, const char* val)        { store_string(iter, val); }
static inline void store_val(DBusMessageIter* iter, const std::string& val) { store_string(iter, val.c_str()); }

/* Templates for vector and map */
template <typename T> struct abrt_dbus_type {};
//template <> struct abrt_dbus_type<bool>        { static const char* csig() { return "b"; } };
template <> struct abrt_dbus_type<int32_t>     { static const char* csig() { return "i"; } static std::string sig(); };
template <> struct abrt_dbus_type<uint32_t>    { static const char* csig() { return "u"; } static std::string sig(); };
template <> struct abrt_dbus_type<int64_t>     { static const char* csig() { return "x"; } static std::string sig(); };
template <> struct abrt_dbus_type<uint64_t>    { static const char* csig() { return "t"; } static std::string sig(); };
template <> struct abrt_dbus_type<std::string> { static const char* csig() { return "s"; } static std::string sig(); };
#define ABRT_DBUS_SIG(T) (abrt_dbus_type<T>::csig() ? abrt_dbus_type<T>::csig() : abrt_dbus_type<T>::sig().c_str())
template <typename E>
struct abrt_dbus_type< std::vector<E> > {
    static const char* csig() { return NULL; }
    static std::string sig() { return ssprintf("a%s", ABRT_DBUS_SIG(E)); }
};
template <typename K, typename V>
struct abrt_dbus_type< std::map<K,V> > {
    static const char* csig() { return NULL; }
    static std::string sig() { return ssprintf("a{%s%s}", ABRT_DBUS_SIG(K), ABRT_DBUS_SIG(V)); }
};

template<typename E>
static void store_vector(DBusMessageIter* iter, const std::vector<E>& val)
{
    DBusMessageIter sub_iter;
    if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, ABRT_DBUS_SIG(E), &sub_iter))
        die_out_of_memory();

    typename std::vector<E>::const_iterator vit = val.begin();
    for (; vit != val.end(); ++vit)
    {
        store_val(&sub_iter, *vit);
    }

    if (!dbus_message_iter_close_container(iter, &sub_iter))
        die_out_of_memory();
}
/*
template<>
static void store_vector(DBus::MessageIter &iter, const std::vector<uint8_t>& val)
{
    if we use such vector, MUST add specialized code here (see in dbus-c++ source)
}
*/
template<typename K, typename V>
static void store_map(DBusMessageIter* iter, const std::map<K,V>& val)
{
    DBusMessageIter sub_iter;
    if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
                ssprintf("{%s%s}", ABRT_DBUS_SIG(K), ABRT_DBUS_SIG(V)).c_str(),
                &sub_iter))
        die_out_of_memory();

    typename std::map<K,V>::const_iterator mit = val.begin();
    for (; mit != val.end(); ++mit)
    {
        DBusMessageIter sub_sub_iter;
        if (!dbus_message_iter_open_container(&sub_iter, DBUS_TYPE_DICT_ENTRY, NULL, &sub_sub_iter))
            die_out_of_memory();
        store_val(&sub_sub_iter, mit->first);
        store_val(&sub_sub_iter, mit->second);
        if (!dbus_message_iter_close_container(&sub_iter, &sub_sub_iter))
            die_out_of_memory();
    }

    if (!dbus_message_iter_close_container(iter, &sub_iter))
        die_out_of_memory();
}

template<typename E>
static inline void store_val(DBusMessageIter* iter, const std::vector<E>& val) { store_vector(iter, val); }
template<typename K, typename V>
static inline void store_val(DBusMessageIter* iter, const std::map<K,V>& val)  { store_map(iter, val); }


/*
 * Helpers for parsing DBus messages
 */

enum {
    ABRT_DBUS_ERROR = -1,
    ABRT_DBUS_LAST_FIELD = 0,
    ABRT_DBUS_MORE_FIELDS = 1,
};
/* Checks type, loads data, advances to the next arg.
 * Returns TRUE if next arg exists.
 */
//int load_bool(DBusMessageIter* iter, bool& val);
int load_int32(DBusMessageIter* iter, int32_t &val);
int load_uint32(DBusMessageIter* iter, uint32_t &val);
int load_int64(DBusMessageIter* iter, int64_t &val);
int load_uint64(DBusMessageIter* iter, uint64_t &val);
int load_charp(DBusMessageIter* iter, const char*& val);
//static inline int load_val(DBusMessageIter* iter, bool &val)        { return load_bool(iter, val); }
static inline int load_val(DBusMessageIter* iter, int32_t &val)     { return load_int32(iter, val); }
static inline int load_val(DBusMessageIter* iter, uint32_t &val)    { return load_uint32(iter, val); }
static inline int load_val(DBusMessageIter* iter, int64_t &val)     { return load_int64(iter, val); }
static inline int load_val(DBusMessageIter* iter, uint64_t &val)    { return load_uint64(iter, val); }
static inline int load_val(DBusMessageIter* iter, const char*& val) { return load_charp(iter, val); }
static inline int load_val(DBusMessageIter* iter, std::string& val)
{
    const char* str;
    int r = load_charp(iter, str);
    val = str;
    return r;
}

/* Templates for vector and map */
template<typename E>
static int load_vector(DBusMessageIter* iter, std::vector<E>& val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_ARRAY)
    {
        error_msg("array expected in dbus message, but not found ('%c')", type);
        return -1;
    }

    DBusMessageIter sub_iter;
    dbus_message_iter_recurse(iter, &sub_iter);

    int r;
//int cnt = 0;
    //type = dbus_message_iter_get_arg_type(&sub_iter);
    /* here "type" is the element's type, and it will be checked by load_val */
    // if (type != DBUS_TYPE_INVALID) - not needed?
    do {
        E elem;
//cnt++;
        r = load_val(&sub_iter, elem);
        if (r < 0)
            return r;
        val.push_back(elem);
    } while (r == ABRT_DBUS_MORE_FIELDS);
//log("%s: %d elems", __func__, cnt);

    return dbus_message_iter_next(iter);
}
/*
template<>
static int load_vector(DBusMessageIter* iter, std::vector<uint8_t>& val)
{
    if we use such vector, MUST add specialized code here (see in dbus-c++ source)
}
*/
template<typename K, typename V>
static int load_map(DBusMessageIter* iter, std::map<K,V>& val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_ARRAY)
    {
        error_msg("array expected in dbus message, but not found ('%c')", type);
        return -1;
    }

    DBusMessageIter sub_iter;
    dbus_message_iter_recurse(iter, &sub_iter);

    bool next_exists;
    int r;
//int cnt = 0;
    do {
        type = dbus_message_iter_get_arg_type(&sub_iter);
        if (type != DBUS_TYPE_DICT_ENTRY)
        {
            error_msg("sub_iter type is not DBUS_TYPE_DICT_ENTRY (%c)!", type);
            return -1;
        }

        DBusMessageIter sub_sub_iter;
        dbus_message_iter_recurse(&sub_iter, &sub_sub_iter);

        K key;
        r = load_val(&sub_sub_iter, key);
        if (r != ABRT_DBUS_MORE_FIELDS)
        {
            if (r == ABRT_DBUS_LAST_FIELD)
                error_msg("malformed map element in dbus message");
            return -1;
        }
        V value;
        r = load_val(&sub_sub_iter, value);
        if (r != ABRT_DBUS_LAST_FIELD)
        {
            if (r == ABRT_DBUS_MORE_FIELDS)
                error_msg("malformed map element in dbus message");
            return -1;
        }
        val[key] = value;
//cnt++;
        next_exists = dbus_message_iter_next(&sub_iter);
    } while (next_exists);
//log("%s: %d elems", __func__, cnt);

    return dbus_message_iter_next(iter);
}

template<typename E>
static inline int load_val(DBusMessageIter* iter, std::vector<E>& val) { return load_vector(iter, val); }
template<typename K, typename V>
static inline int load_val(DBusMessageIter* iter, std::map<K,V>& val)  { return load_map(iter, val); }
