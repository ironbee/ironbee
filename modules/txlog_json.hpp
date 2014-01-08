/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee Modules --- Transaction Logs JSON Builder
 *
 * How to build a JSON file for a Transaction Log.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef __MODULES__TXLOG_JSON_HPP__
#define __MODULES__TXLOG_JSON_HPP__

#include <ironbeepp/exception.hpp>

#include <ironbee/field.h>

#include <boost/any.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include <yajl/yajl_common.h>
#include <yajl/yajl_gen.h>

/**
 * An alternative buffer, we control, to the default YAJL buffer.
 *
 * This allows TxLogJson to divorce a malloc-backed
 * buffer containing JSON from the yajl_gen generator that
 * rendered it so that the buffer will exist past the lifetime
 * of the generator.
 */
class TxLogJsonBuffer : public boost::noncopyable {
public:
    //! Constructor.
    TxLogJsonBuffer();

    /**
     * Free C resources (memory) acquired over the lifetime of this obj.
     *
     * Any buffer given to the user by a call to
     * TxLogJsonBuffer::divorce_buffer() is not freed.
     */
    ~TxLogJsonBuffer();

    /**
     * Extend the internal buffer by @a len and append @a str.
     *
     * This allows C-implemented clients to this class to
     * append without building a new std::string instance.
     *
     * @param[in] str The string to append.
     * @param[in] str_len The length of the string to append.

     * @throws TxLogJsonError on failures.
     */
    void append(const char* str, size_t str_len);

    /**
     * Divorce the malloc buffer from this object.
     *
     * The buffer will not be free'ed at object destruction time
     * and is the user's responsibility to free() the buffer.
     *
     * After this is called, the internal buffer is NULL and all
     * size and length values are 0.
     *
     * @param[out] buf The buffer.
     * @param[out] buf_sz The buffer size.
     */
    void divorce_buffer(char*& buf, size_t& buf_sz);

private:
    //! Length of the string in m_json_buffer.
    size_t m_json_buffer_len;

    //! The size of m_json_buffer.
    size_t m_json_buffer_sz;

    //! Buffer containing the currently rendered JSON.
    char* m_json_buffer;
};

// Forward define classes that reference each other.
template <typename PARENT> class TxLogJsonMap;
template <typename PARENT> class TxLogJsonArray;

//! Any error in TxLogJson.
struct TxLogJsonError : public IronBee::eother {};

/**
 * A rendering wrapper around YAJL's JSON generation api.
 *
 * This class creates a JSON generator backed by a TxLogJsonBuffer.
 * The user may make calls against this class to append
 * JSON information to the buffer using the same semantics as
 * the YAJL API presents.
 *
 * User's may also use the TxLogJsonArray and TxLogJsonMap
 * instances returned by TxLogJson::withArray() and TxLogJson::withMap()
 * to construct JSON in a builder pattern. Notice that arrays and maps
 * must be closed to produce valid JSON.
 *
 * For example, this
 *
 * @code
 * TxLogJson()
 *     .withMap()
 *         .withArray("Array1")
 *             .withString("Value 1")
 *             .withString("Value 2")
 *         .close()
 *         .withString("String1", "Value 3")
 *     .close()
 *     .render(&str, &str_len);
 * @endcode
 *
 * Is equivalent to this
 *
 * @code
 * TxLogJson txLogJson = TxLogJson();
 * TxLogJsonMap map = TxLogJson.withMap();
 * TxLogJsonArray array = map.withArray("Array1");
 *
 * array.withString("Value 1");
 * array.withString("Value 2");
 * array.close();
 *
 * map.withString("String1", "Value 3");
 * map.close()
 *
 * txLogJson.render(&str, &str_len);
 * @endcode
 *
 * @note This class does as much validation as YAJL, which is almost nothing.
 *       It is trivial to produce invalid JSON by not closing maps and arrays.
 */
class TxLogJson : boost::noncopyable {
public:
    //! The generator type.
    typedef yajl_gen json_generator_t;

private:
    //! The buffer we will render into.
    TxLogJsonBuffer  m_buffer;

    //! A protective wrapper for @c m_buffer used to pass it to C.
    boost::any       m_any;

    //! The yajl_gen instance.
    json_generator_t m_json_generator;

public:
    //! Constructor.
    TxLogJson();

    //! Destructor.
    ~TxLogJson();

    //! Render a boost::posix_time::ptime in a standard way.
    void withTime(const boost::posix_time::ptime& val);

    //! Render a string.
    void withString(const std::string& val);

    //! Render a C string.
    void withString(const char* val);

    //! Render a C string.
    void withString(const char* val, size_t len);

    //! Render a integer.
    void withInt(int val);

    //! Render a double.
    void withDouble(double val);

    //! Render a boolean.
    void withBool(bool val);

    //! Render a NULL.
    void withNull();

    //! Accessor for the JSON Generator (YAJL).
    json_generator_t& getJsonGenerator() { return m_json_generator; }

    //! Render and return a map that, when closed, will return @c this.
    TxLogJsonMap<TxLogJson> withMap();

    //! Render and return an array that, when closed, will return @c this.
    TxLogJsonArray<TxLogJson> withArray();

    /**
     * Render the JSON to the buffer and return it to @a buf and @ buf_sz.
     *
     * Rendering divorces the internal buffer of this class
     * from it, requiring the caller to call free() on @a buf
     * when the caller is done with it.
     *
     * Calling render() leaves @c this with an empty buffer, allowing
     * for a fresh rendering of JSON to the empty buffer.
     *
     * @note @a buf must be passed to free() or it is a memory leak.
     *
     * @param[out] buf Malloc'ed buffer of rendered JSON. This must be passed
     *             to free() by the caller. This is not a null-terminated string.
     * @param[out] buf_sz The size of @a buf.
     */
    void render(char*& buf, size_t& buf_sz);
};

/**
 * A builder-patterned class for building JSON maps.
 *
 * @tparam PARENT The enclosing JSON structure (another TxLogJsonMap or
 *         TxLogJsonArray) of the outer-most TxLogJson object.
 *         This is what is returned by TxLogJsonMap::close().
 */
template <typename PARENT>
class TxLogJsonMap
{
    template <typename T> friend class TxLogJsonArray;
    template <typename T> friend class TxLogJsonMap;
    friend class TxLogJson;

public:

    /**
     * Copy constructor.
     *
     * Public because the only other constructor is private. No need
     * for the rule-of-three in this case.
     */
    TxLogJsonMap(const TxLogJsonMap& map);

    /**
     * Close this collection and return the parent.
     */
    PARENT& close();

    //! Begin rendering an array at the map entry @a name.
    TxLogJsonArray<TxLogJsonMap<PARENT> > withArray(const char* name);

    //! Begin rendering an map at the map entry @a name.
    TxLogJsonMap<TxLogJsonMap<PARENT> > withMap(const char* name);

    //! Render the time @a val under entry @a name.
    TxLogJsonMap<PARENT>& withTime(
        const char* name,
        const boost::posix_time::ptime& val
    );

    //! Render @a val under the map entry @a name.
    TxLogJsonMap<PARENT>& withString(const char* name, const std::string& val);

    //! Render @a val under the map entry @a name.
    TxLogJsonMap<PARENT>& withString(const char* name, const char* val);

    //! Render @a val under the map entry @a name.
    TxLogJsonMap<PARENT>& withString(
       const char* name,
       const char* val,
       size_t len);

    //! Render @a val under the map entry @a name.
    TxLogJsonMap<PARENT>& withInt(const char* name, int val);

    //! Render @a val under the map entry @a name.
    TxLogJsonMap<PARENT>& withDouble(const char* name, double val);

    //! Render @a val under the map entry @a name.
    TxLogJsonMap<PARENT>& withBool(const char* name, bool val);

    //! Render a null entry under the map entry @a name.
    TxLogJsonMap<PARENT>& withNull(const char* name);

    /**
     * Call the given function, @a f, on this object's TxLogJson.
     *
     * This allows for custom rendering of elements, or rendering
     * a dynamic number of elements without disrupting the
     * Fluent pattern.
     *
     * @note A key is not rendered. Users of this should call
     * TxLogJson::withString() to generate a key themselves.
     */
    TxLogJsonMap<PARENT>& withFunction(
        boost::function<void(TxLogJson& txLogJson)> f
    );

private:
    //! What to use for rendering JSON atoms.
    TxLogJson& m_txLogJson;

    //! The class that generated this object. Returned by @c close().
    PARENT&    m_parent;

    /**
     * Constructor.
     *
     * @param[in] txLogJson The generator that all JSON is generated with.
     * @param[in] parent The creating object. This is returned by
     *            TxLogJsonMap::close().
     */
    TxLogJsonMap(TxLogJson& txLogJson, PARENT& parent);
};

/**
 * A builder-patterned class for building JSON arrays.
 *
 * @tparam PARENT The enclosing JSON structure (another TxLogJsonMap or
 *         TxLogJsonArray) of the outer-most TxLogJson object.
 *         This is what is returned by TxLogJsonArray::close().
 */
template <typename PARENT>
class TxLogJsonArray  {
    template <typename T> friend class TxLogJsonArray;
    template <typename T> friend class TxLogJsonMap;
    friend class TxLogJson;

private:
    //! What to use for rendering JSON atoms.
    TxLogJson& m_txLogJson;

    //! The class that generated this object. Returned by @c close().
    PARENT& m_parent;

    /**
     * Constructor.
     *
     * @param[in] txLogJson The generator that all JSON is generated with.
     * @param[in] parent The creating object. This is returned by
     *            TxLogJsonArray::close().
     */
    TxLogJsonArray(TxLogJson& txLogJson, PARENT& parent);

public:

    /**
     * Copy constructor.
     *
     * Public because the only other constructor is private. No need
     * for the rule-of-three in this case.
     */
    TxLogJsonArray(const TxLogJsonArray& array);

    /**
     * Close this collection and return the parent.
     */
    PARENT& close();

    //! Begin rendering an array in this array.
    TxLogJsonArray<TxLogJsonArray<PARENT> > withArray();

    //! Begin rendering an map in this array.
    TxLogJsonMap<TxLogJsonArray<PARENT> > withMap();

    //! Render @a val as an element of this array.
    TxLogJsonArray<PARENT>& withTime(const boost::posix_time::ptime& val);

    //! Render @a val as an element of this array.
    TxLogJsonArray<PARENT>& withString(const std::string& val);

    //! Render @a val as an element of this array.
    TxLogJsonArray<PARENT>& withString(const char* val);

    //! Render @a val as an element of this array.
    TxLogJsonArray<PARENT>& withString(const char* val, size_t len);

    //! Render @a val as an element of this array.
    TxLogJsonArray<PARENT>& withInt(int val);

    //! Render @a val as an element of this array.
    TxLogJsonArray<PARENT>& withDouble(double val);

    //! Render @a val as an element of this array.
    TxLogJsonArray<PARENT>& withBool(bool val);

    //! Render @a val as an element of this array.
    TxLogJsonArray<PARENT>& withNull();

    /**
     * Call the given function, @a f, on this object's TxLogJson.
     *
     * This allows for custom rendering of elements, or rendering
     * a dynamic number of elements without disrupting the
     * Fluent pattern.
     */
    TxLogJsonArray<PARENT>& withFunction(
        boost::function<void(TxLogJson& txLogJson)> f
    );
};

template <typename PARENT>
TxLogJsonArray<PARENT>::TxLogJsonArray(const TxLogJsonArray<PARENT>& array) :
    m_txLogJson(array.m_txLogJson),
    m_parent(array.m_parent)
{}

template <typename PARENT>
TxLogJsonMap<PARENT>::TxLogJsonMap(const TxLogJsonMap<PARENT> &map) :
    m_txLogJson(map.m_txLogJson),
    m_parent(map.m_parent)
{}

template <typename PARENT>
TxLogJsonArray<PARENT>::TxLogJsonArray
(
    TxLogJson& txLogJson,
    PARENT&    parent
) :
    m_txLogJson(txLogJson),
    m_parent(parent)
{
    int yg_rc = yajl_gen_array_open(m_txLogJson.getJsonGenerator());
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(TxLogJsonError() <<
            IronBee::errinfo_what("Failed to open array."));
    }
}

template <typename PARENT>
PARENT& TxLogJsonArray<PARENT>::close()
{
    int yg_rc = yajl_gen_array_close(m_txLogJson.getJsonGenerator());
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(TxLogJsonError() <<
            IronBee::errinfo_what("Failed close array."));
    }

    return m_parent;
}

template <typename PARENT>
TxLogJsonMap<TxLogJsonArray<PARENT> >
TxLogJsonArray<PARENT>::withMap()
{
    return TxLogJsonMap<TxLogJsonArray<PARENT> >(m_txLogJson, *this);
}

template <typename PARENT>
TxLogJsonArray<TxLogJsonArray<PARENT> >
TxLogJsonArray<PARENT>::withArray()
{
    return TxLogJsonArray<TxLogJsonArray<PARENT> >(m_txLogJson, *this);
}

template <typename PARENT>
TxLogJsonMap<PARENT>::TxLogJsonMap(
    TxLogJson& txLogJson,
    PARENT&    parent)
:
    m_txLogJson(txLogJson),
    m_parent(parent)
{
    int yg_rc = yajl_gen_map_open(txLogJson.getJsonGenerator());
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(TxLogJsonError() <<
            IronBee::errinfo_what("Failed to open map"));
    }
}

template <typename PARENT>
PARENT& TxLogJsonMap<PARENT>::close()
{
    int yg_rc = yajl_gen_map_close(m_txLogJson.getJsonGenerator());
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(TxLogJsonError() <<
            IronBee::errinfo_what("Failed to open map"));
    }

    return m_parent;
}

template <typename PARENT>
TxLogJsonMap<TxLogJsonMap<PARENT> >
TxLogJsonMap<PARENT>::withMap(const char* name)
{
    m_txLogJson.withString(name);

    return TxLogJsonMap<TxLogJsonMap<PARENT> >(m_txLogJson, *this);
}

template <typename PARENT>
TxLogJsonArray<TxLogJsonMap<PARENT> >
TxLogJsonMap<PARENT>::withArray(const char* name)
{
    m_txLogJson.withString(name);

    return TxLogJsonArray<TxLogJsonMap<PARENT> >(m_txLogJson, *this);
}

template <typename PARENT>
TxLogJsonMap<PARENT>& TxLogJsonMap<PARENT>::withTime(
    const char*                     name,
    const boost::posix_time::ptime& val
)
{
    m_txLogJson.withString(name);
    m_txLogJson.withTime(val);
    return *this;
}

template <typename PARENT>
TxLogJsonMap<PARENT>& TxLogJsonMap<PARENT>::withString(
    const char*        name,
    const std::string& val
)
{
    m_txLogJson.withString(name);
    m_txLogJson.withString(val);
    return *this;
}

template <typename PARENT>
TxLogJsonMap<PARENT>& TxLogJsonMap<PARENT>::withString(
    const char* name,
    const char* val
)
{
    m_txLogJson.withString(name);
    m_txLogJson.withString(val);
    return *this;

}

template <typename PARENT>
TxLogJsonMap<PARENT>& TxLogJsonMap<PARENT>::withString(
    const char* name,
    const char* val,
    size_t      len
)
{
    m_txLogJson.withString(name);
    m_txLogJson.withString(val, len);
    return *this;
}

template <typename PARENT>
TxLogJsonMap<PARENT>& TxLogJsonMap<PARENT>::withInt(
    const char* name,
    int         val
)
{
    m_txLogJson.withString(name);
    m_txLogJson.withInt(val);
    return *this;
}

template <typename PARENT>
TxLogJsonMap<PARENT>& TxLogJsonMap<PARENT>::withNull(
    const char* name
)
{
    m_txLogJson.withString(name);
    m_txLogJson.withNull();
    return *this;
}

template <typename PARENT>
TxLogJsonMap<PARENT>& TxLogJsonMap<PARENT>::withDouble(
    const char* name,
    double      val
)
{
    m_txLogJson.withString(name);
    m_txLogJson.withDouble(val);
    return *this;
}
template <typename PARENT>
TxLogJsonMap<PARENT>& TxLogJsonMap<PARENT>::withBool(
    const char* name,
    bool        val
)
{
    m_txLogJson.withString(name);
    m_txLogJson.withBool(val);
    return *this;
}

template <typename PARENT>
TxLogJsonMap<PARENT>& TxLogJsonMap<PARENT>::withFunction(
        boost::function<void(TxLogJson& txLogJson)> f
)
{
    f(this->m_txLogJson);
    return *this;
}

template <typename PARENT>
TxLogJsonArray<PARENT>& TxLogJsonArray<PARENT>::withTime(const boost::posix_time::ptime& val)
{
    m_txLogJson.withTime(val);
    return *this;
}

template <typename PARENT>
TxLogJsonArray<PARENT>& TxLogJsonArray<PARENT>::withString(const std::string& val)
{
    m_txLogJson.withString(val);
    return *this;
}

template <typename PARENT>
TxLogJsonArray<PARENT>& TxLogJsonArray<PARENT>::withString(const char* val)
{
    m_txLogJson.withString(val);
    return *this;
}

template <typename PARENT>
TxLogJsonArray<PARENT>& TxLogJsonArray<PARENT>::withString(const char* val, size_t len)
{
    m_txLogJson.withString(val, len);
    return *this;
}

template <typename PARENT>
TxLogJsonArray<PARENT>& TxLogJsonArray<PARENT>::withInt(int val)
{
    m_txLogJson.withInt(val);
    return *this;
}

template <typename PARENT>
TxLogJsonArray<PARENT>& TxLogJsonArray<PARENT>::withDouble(double val)
{
    m_txLogJson.withDouble(val);
    return *this;
}

template <typename PARENT>
TxLogJsonArray<PARENT>& TxLogJsonArray<PARENT>::withBool(bool val)
{
    m_txLogJson.withBool(val);
    return *this;
}

template <typename PARENT>
TxLogJsonArray<PARENT>& TxLogJsonArray<PARENT>::withNull()
{
    m_txLogJson.withNull();
    return *this;
}

template <typename PARENT>
TxLogJsonArray<PARENT>& TxLogJsonArray<PARENT>::withFunction(
        boost::function<void(TxLogJson& txLogJson)> f
)
{
    f(this->m_txLogJson);
    return *this;
}

#endif /* __MODULES__TxLOG_JSON_HPP__ */
