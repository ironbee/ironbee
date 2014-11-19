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
 * @brief IronBee++ --- JSON API
 *
 * This provides a C++ style JSON API.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#ifndef __IBPP__JSON__
#define __IBPP__JSON__

#include <ironbeepp/abi_compatibility.hpp>

#include <ironbeepp/exception.hpp>

#include <boost/any.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include <yajl/yajl_common.h>
#include <yajl/yajl_gen.h>

namespace IronBee {

/**
 * An alternative buffer, we control, to the default YAJL buffer.
 *
 * This allows Json to divorce a malloc-backed
 * buffer containing JSON from the yajl_gen generator that
 * rendered it so that the buffer will exist past the lifetime
 * of the generator.
 */
class JsonBuffer : public boost::noncopyable {
public:
    //! Constructor.
    JsonBuffer();

    /**
     * Free C resources (memory) acquired over the lifetime of this obj.
     *
     * Any buffer given to the user by a call to
     * JsonBuffer::divorce_buffer() is not freed.
     */
    ~JsonBuffer();

    /**
     * Extend the internal buffer by @a len and append @a str.
     *
     * This allows C-implemented clients to this class to
     * append without building a new std::string instance.
     *
     * @param[in] str The string to append.
     * @param[in] str_len The length of the string to append.

     * @throws JsonError on failures.
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
template <typename PARENT> class JsonMap;
template <typename PARENT> class JsonArray;

//! Any error in Json.
struct JsonError : public IronBee::eother {};

/**
 * A rendering wrapper around YAJL's JSON generation api.
 *
 * This class creates a JSON generator backed by a JsonBuffer.
 * The user may make calls against this class to append
 * JSON information to the buffer using the same semantics as
 * the YAJL API presents.
 *
 * User's may also use the JsonArray and JsonMap
 * instances returned by Json::withArray() and Json::withMap()
 * to construct JSON in a builder pattern. Notice that arrays and maps
 * must be closed to produce valid JSON.
 *
 * For example, this
 *
 * @code
 * Json()
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
 * Json Json = Json();
 * JsonMap map = Json.withMap();
 * JsonArray array = map.withArray("Array1");
 *
 * array.withString("Value 1");
 * array.withString("Value 2");
 * array.close();
 *
 * map.withString("String1", "Value 3");
 * map.close()
 *
 * Json.render(&str, &str_len);
 * @endcode
 *
 * @note This class does as much validation as YAJL, which is almost nothing.
 *       It is trivial to produce invalid JSON by not closing maps and arrays.
 */
class Json : boost::noncopyable {
public:
    //! The generator type.
    typedef yajl_gen json_generator_t;

private:
    //! The buffer we will render into.
    JsonBuffer  m_buffer;

    //! A protective wrapper for @c m_buffer used to pass it to C.
    boost::any       m_any;

    //! The yajl_gen instance.
    json_generator_t m_json_generator;

public:
    //! Constructor.
    Json();

    //! Destructor.
    ~Json();

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
    JsonMap<Json> withMap();

    //! Render and return an array that, when closed, will return @c this.
    JsonArray<Json> withArray();

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
 * @tparam PARENT The enclosing JSON structure (another JsonMap or
 *         JsonArray) of the outer-most Json object.
 *         This is what is returned by JsonMap::close().
 */
template <typename PARENT>
class JsonMap
{
    template <typename T> friend class JsonArray;
    template <typename T> friend class JsonMap;
    friend class Json;

public:

    /**
     * Copy constructor.
     *
     * Public because the only other constructor is private. No need
     * for the rule-of-three in this case.
     */
    JsonMap(const JsonMap& map);

    /**
     * Close this collection and return the parent.
     */
    PARENT& close();

    //! Begin rendering an array at the map entry @a name.
    JsonArray<JsonMap<PARENT> > withArray(const char* name);

    //! Begin rendering an map at the map entry @a name.
    JsonMap<JsonMap<PARENT> > withMap(const char* name);

    //! Render the time @a val under entry @a name.
    JsonMap<PARENT>& withTime(
        const char* name,
        const boost::posix_time::ptime& val
    );

    //! Render @a val under the map entry @a name.
    JsonMap<PARENT>& withString(const char* name, const std::string& val);

    //! Render @a val under the map entry @a name.
    JsonMap<PARENT>& withString(const char* name, const char* val);

    //! Render @a val under the map entry @a name.
    JsonMap<PARENT>& withString(
       const char* name,
       const char* val,
       size_t len);

    //! Render @a val under the map entry @a name.
    JsonMap<PARENT>& withInt(const char* name, int val);

    //! Render @a val under the map entry @a name.
    JsonMap<PARENT>& withDouble(const char* name, double val);

    //! Render @a val under the map entry @a name.
    JsonMap<PARENT>& withBool(const char* name, bool val);

    //! Render a null entry under the map entry @a name.
    JsonMap<PARENT>& withNull(const char* name);

    /**
     * Call the given function, @a f, on this object's Json.
     *
     * This allows for custom rendering of elements, or rendering
     * a dynamic number of elements without disrupting the
     * Fluent pattern.
     *
     * @note A key is not rendered. Users of this should call
     * Json::withString() to generate a key themselves.
     */
    JsonMap<PARENT>& withFunction(
        boost::function<void(Json& Json)> f
    );

private:
    //! What to use for rendering JSON atoms.
    Json& m_Json;

    //! The class that generated this object. Returned by @c close().
    PARENT&    m_parent;

    /**
     * Constructor.
     *
     * @param[in] Json The generator that all JSON is generated with.
     * @param[in] parent The creating object. This is returned by
     *            JsonMap::close().
     */
    JsonMap(Json& Json, PARENT& parent);
};

/**
 * A builder-patterned class for building JSON arrays.
 *
 * @tparam PARENT The enclosing JSON structure (another JsonMap or
 *         JsonArray) of the outer-most Json object.
 *         This is what is returned by JsonArray::close().
 */
template <typename PARENT>
class JsonArray  {
    template <typename T> friend class JsonArray;
    template <typename T> friend class JsonMap;
    friend class Json;

private:
    //! What to use for rendering JSON atoms.
    Json& m_Json;

    //! The class that generated this object. Returned by @c close().
    PARENT& m_parent;

    /**
     * Constructor.
     *
     * @param[in] Json The generator that all JSON is generated with.
     * @param[in] parent The creating object. This is returned by
     *            JsonArray::close().
     */
    JsonArray(Json& Json, PARENT& parent);

public:

    /**
     * Copy constructor.
     *
     * Public because the only other constructor is private. No need
     * for the rule-of-three in this case.
     */
    JsonArray(const JsonArray& array);

    /**
     * Close this collection and return the parent.
     */
    PARENT& close();

    //! Begin rendering an array in this array.
    JsonArray<JsonArray<PARENT> > withArray();

    //! Begin rendering an map in this array.
    JsonMap<JsonArray<PARENT> > withMap();

    //! Render @a val as an element of this array.
    JsonArray<PARENT>& withTime(const boost::posix_time::ptime& val);

    //! Render @a val as an element of this array.
    JsonArray<PARENT>& withString(const std::string& val);

    //! Render @a val as an element of this array.
    JsonArray<PARENT>& withString(const char* val);

    //! Render @a val as an element of this array.
    JsonArray<PARENT>& withString(const char* val, size_t len);

    //! Render @a val as an element of this array.
    JsonArray<PARENT>& withInt(int val);

    //! Render @a val as an element of this array.
    JsonArray<PARENT>& withDouble(double val);

    //! Render @a val as an element of this array.
    JsonArray<PARENT>& withBool(bool val);

    //! Render @a val as an element of this array.
    JsonArray<PARENT>& withNull();

    /**
     * Call the given function, @a f, on this object's Json.
     *
     * This allows for custom rendering of elements, or rendering
     * a dynamic number of elements without disrupting the
     * Fluent pattern.
     */
    JsonArray<PARENT>& withFunction(
        boost::function<void(Json& Json)> f
    );
};

template <typename PARENT>
JsonArray<PARENT>::JsonArray(const JsonArray<PARENT>& array) :
    m_Json(array.m_Json),
    m_parent(array.m_parent)
{}

template <typename PARENT>
JsonMap<PARENT>::JsonMap(const JsonMap<PARENT> &map) :
    m_Json(map.m_Json),
    m_parent(map.m_parent)
{}

template <typename PARENT>
JsonArray<PARENT>::JsonArray
(
    Json& Json,
    PARENT&    parent
) :
    m_Json(Json),
    m_parent(parent)
{
    int yg_rc = yajl_gen_array_open(m_Json.getJsonGenerator());
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(JsonError() <<
            IronBee::errinfo_what("Failed to open array."));
    }
}

template <typename PARENT>
PARENT& JsonArray<PARENT>::close()
{
    int yg_rc = yajl_gen_array_close(m_Json.getJsonGenerator());
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(JsonError() <<
            IronBee::errinfo_what("Failed close array."));
    }

    return m_parent;
}

template <typename PARENT>
JsonMap<JsonArray<PARENT> >
JsonArray<PARENT>::withMap()
{
    return JsonMap<JsonArray<PARENT> >(m_Json, *this);
}

template <typename PARENT>
JsonArray<JsonArray<PARENT> >
JsonArray<PARENT>::withArray()
{
    return JsonArray<JsonArray<PARENT> >(m_Json, *this);
}

template <typename PARENT>
JsonMap<PARENT>::JsonMap(
    Json& Json,
    PARENT&    parent)
:
    m_Json(Json),
    m_parent(parent)
{
    int yg_rc = yajl_gen_map_open(Json.getJsonGenerator());
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(JsonError() <<
            IronBee::errinfo_what("Failed to open map"));
    }
}

template <typename PARENT>
PARENT& JsonMap<PARENT>::close()
{
    int yg_rc = yajl_gen_map_close(m_Json.getJsonGenerator());
    if (yg_rc != yajl_gen_status_ok) {
        BOOST_THROW_EXCEPTION(JsonError() <<
            IronBee::errinfo_what("Failed to open map"));
    }

    return m_parent;
}

template <typename PARENT>
JsonMap<JsonMap<PARENT> >
JsonMap<PARENT>::withMap(const char* name)
{
    m_Json.withString(name);

    return JsonMap<JsonMap<PARENT> >(m_Json, *this);
}

template <typename PARENT>
JsonArray<JsonMap<PARENT> >
JsonMap<PARENT>::withArray(const char* name)
{
    m_Json.withString(name);

    return JsonArray<JsonMap<PARENT> >(m_Json, *this);
}

template <typename PARENT>
JsonMap<PARENT>& JsonMap<PARENT>::withTime(
    const char*                     name,
    const boost::posix_time::ptime& val
)
{
    m_Json.withString(name);
    m_Json.withTime(val);
    return *this;
}

template <typename PARENT>
JsonMap<PARENT>& JsonMap<PARENT>::withString(
    const char*        name,
    const std::string& val
)
{
    m_Json.withString(name);
    m_Json.withString(val);
    return *this;
}

template <typename PARENT>
JsonMap<PARENT>& JsonMap<PARENT>::withString(
    const char* name,
    const char* val
)
{
    m_Json.withString(name);
    m_Json.withString(val);
    return *this;

}

template <typename PARENT>
JsonMap<PARENT>& JsonMap<PARENT>::withString(
    const char* name,
    const char* val,
    size_t      len
)
{
    m_Json.withString(name);
    m_Json.withString(val, len);
    return *this;
}

template <typename PARENT>
JsonMap<PARENT>& JsonMap<PARENT>::withInt(
    const char* name,
    int         val
)
{
    m_Json.withString(name);
    m_Json.withInt(val);
    return *this;
}

template <typename PARENT>
JsonMap<PARENT>& JsonMap<PARENT>::withNull(
    const char* name
)
{
    m_Json.withString(name);
    m_Json.withNull();
    return *this;
}

template <typename PARENT>
JsonMap<PARENT>& JsonMap<PARENT>::withDouble(
    const char* name,
    double      val
)
{
    m_Json.withString(name);
    m_Json.withDouble(val);
    return *this;
}
template <typename PARENT>
JsonMap<PARENT>& JsonMap<PARENT>::withBool(
    const char* name,
    bool        val
)
{
    m_Json.withString(name);
    m_Json.withBool(val);
    return *this;
}

template <typename PARENT>
JsonMap<PARENT>& JsonMap<PARENT>::withFunction(
        boost::function<void(Json& Json)> f
)
{
    f(this->m_Json);
    return *this;
}

template <typename PARENT>
JsonArray<PARENT>& JsonArray<PARENT>::withTime(const boost::posix_time::ptime& val)
{
    m_Json.withTime(val);
    return *this;
}

template <typename PARENT>
JsonArray<PARENT>& JsonArray<PARENT>::withString(const std::string& val)
{
    m_Json.withString(val);
    return *this;
}

template <typename PARENT>
JsonArray<PARENT>& JsonArray<PARENT>::withString(const char* val)
{
    m_Json.withString(val);
    return *this;
}

template <typename PARENT>
JsonArray<PARENT>& JsonArray<PARENT>::withString(const char* val, size_t len)
{
    m_Json.withString(val, len);
    return *this;
}

template <typename PARENT>
JsonArray<PARENT>& JsonArray<PARENT>::withInt(int val)
{
    m_Json.withInt(val);
    return *this;
}

template <typename PARENT>
JsonArray<PARENT>& JsonArray<PARENT>::withDouble(double val)
{
    m_Json.withDouble(val);
    return *this;
}

template <typename PARENT>
JsonArray<PARENT>& JsonArray<PARENT>::withBool(bool val)
{
    m_Json.withBool(val);
    return *this;
}

template <typename PARENT>
JsonArray<PARENT>& JsonArray<PARENT>::withNull()
{
    m_Json.withNull();
    return *this;
}

template <typename PARENT>
JsonArray<PARENT>& JsonArray<PARENT>::withFunction(
        boost::function<void(Json& Json)> f
)
{
    f(this->m_Json);
    return *this;
}

} // namespace IronBee

#endif // __IBPP__JSON__