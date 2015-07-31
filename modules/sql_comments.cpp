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
 * @brief IronBee --- SQL Module
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbeepp/module.hpp>
#include <ironbeepp/module_delegate.hpp>
#include <ironbeepp/module_bootstrap.hpp>
#include <ironbeepp/memory_manager.hpp>
#include <ironbeepp/transformation.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wc++11-extensions")
#pragma clang diagnostic ignored "-Wc++11-extensions"
#endif
#if __has_warning("-Wunused-local-typedef")
#pragma clang diagnostic ignored "-Wunused-local-typedef"
#endif
#endif
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_statement.hpp>
#include <boost/spirit/include/phoenix_container.hpp>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <string>

using namespace IronBee;

namespace {

using namespace IronBee;
namespace qi = boost::spirit::qi;
namespace phoenix = boost::phoenix;

/**
 * A class to hold a parser, replacement text, and a method to apply them.
 *
 * This class does not define how member field ReplaceComments::m_parser
 * is built. Inheriting classes must do this and re-build that parser
 * on each construction and on each copy. Parsers hold
 * references to sub-parsers and so must be managed accordingly.
 */
class ReplaceComments {
public:
    // Useful iterator type.
    typedef const char * itr_t;

    //! Build a replacement with the empty string.
    ReplaceComments();

    //! Build a replacement with the given text.
    explicit ReplaceComments(const char * replacement);

    //! Copy constructor. This does not copy the parser. You must rebuild it.
    ReplaceComments(const ReplaceComments& that);

    ConstField operator()(
        MemoryManager mm,
        ConstField    field_in
    ) const;

protected:

    //! Test to replace matched comments with.
    std::string m_replacement;

    //! Main parser.
    qi::rule<itr_t, std::string()> m_parser;
};

ReplaceComments::ReplaceComments() : m_replacement("")
{}

ReplaceComments::ReplaceComments(
    const ReplaceComments& that
) :
    m_replacement(that.m_replacement)
{}

ReplaceComments::ReplaceComments(
    const char * replacement
) :
    m_replacement(replacement)
{}

ConstField ReplaceComments::operator()(
    MemoryManager mm,
    ConstField    field_in
) const
{
    itr_t       first;
    itr_t       last;
    bool        parse_success;
    std::string cleaned_text;

    /* Extract text values into iterable types. */
    switch (field_in.type()) {
    case Field::NULL_STRING:
        first = field_in.value_as_null_string();
        if (first == NULL) {
            return field_in;
        }
        last  = first + strlen(first);
        if (last == first) {
            return field_in;
        }
        break;
    case Field::BYTE_STRING:
        first = field_in.value_as_byte_string().const_data();
        if (first == NULL) {
            return field_in;
        }
        last  = first +  field_in.value_as_byte_string().length();
        if (last == first) {
            return field_in;
        }
        break;
    default:
        return field_in;
    }


    /* Parse a single comment. */
    parse_success = qi::phrase_parse(
        // Range to parse.
        first, last,

        // Begin grammar.
        m_parser,
        // End grammar.

        // Skip parser.
        qi::space,

        // Output value.
        cleaned_text
    );

    /* If
     * (1) we fully matched
     * (2) parsed successfully
     * (3) built a string, cleaned_text, that is shorter than the input.
     */
    if (
        first == last &&
        parse_success &&
        cleaned_text.length() != static_cast<size_t>(last-first)
    ) {
        return Field::create_byte_string(
            field_in.memory_manager(),
            field_in.name(),
            field_in.name_length(),
            ByteString::create(mm, cleaned_text)
        );
    }

    return field_in;
}

//! Class to remove PostgreSQL comments.
class PgReplaceComments : public ReplaceComments {
public:
    // Useful iterator type.
    typedef const char * itr_t;

    //! See ReplacementComments.
    PgReplaceComments();

    //! See ReplacementComments.
    explicit PgReplaceComments(const char *replacement);

    //! See ReplacementComments.
    PgReplaceComments(const PgReplaceComments& that);

private:
    //! Construct the parser objects. Called by all constructors.
    void build_parser();

    // Basic symbols.
    //! `/*` literal.
    qi::rule<itr_t> m_open_comment;

    //! `*/` literal.
    qi::rule<itr_t> m_close_comment;

    //! Any character sequence no /* or */.
    qi::rule<itr_t, std::string()> m_non_comment;

    /**
     * Simple comments parser with 2 productions.
     * 1. no embedded comments.
     * 2. embedded comments.
     */
    qi::rule<itr_t, std::string()> m_comments;
};

PgReplaceComments::PgReplaceComments() : ReplaceComments()
{
    build_parser();
}

PgReplaceComments::PgReplaceComments(
    const char *replacement
) :
    ReplaceComments(replacement)
{
    build_parser();
}

// To copy correctly we must re-build our parser.
PgReplaceComments::PgReplaceComments(const PgReplaceComments& that)
:
    ReplaceComments(that)
{
    build_parser();
}

void PgReplaceComments::build_parser()
{

    using boost::spirit::qi::labels::_val;

    // Basic symbols.
    m_open_comment  = qi::lit("/*");
    m_close_comment = qi::lit("*/");
    m_non_comment   = qi::char_ - m_open_comment - m_close_comment;

    // When we find a comment, what do we do? Replace the comment or omit it?
    m_comments =
        qi::as_string[
            m_open_comment >> *m_non_comment >> m_close_comment |
            m_open_comment >> *m_non_comment >> m_comments >> *m_non_comment >> m_close_comment
        ][ _val += phoenix::ref(m_replacement) ];

    // Now setup our comments as part of a larger grammar with repetitions.
    m_parser = qi::as_string[
        qi::no_skip[
            *m_non_comment >> (m_comments % m_non_comment ) >> *m_non_comment
        ]
    ];
}

/**
 * Class to remove MySQL comments.
 *
 * This removes comments of the format:
 * - `-- comment` until the end of the line.
 * - `/+ /+ +/` style invalid comments (where the + is a * because of
 *    C++ commenting rules).
 */
class MysqlReplaceComments : public ReplaceComments {
public:
    // Useful iterator type.
    typedef const char * itr_t;

    //! See ReplacementComments.
    MysqlReplaceComments();

    //! See ReplacementComments.
    explicit MysqlReplaceComments(const char *replacement);

    //! See ReplacementComments.
    MysqlReplaceComments(const MysqlReplaceComments& that);

private:
    //! Construct the parser objects. Called by all constructors.
    void build_parser();

    // Basic symbols.
    //! `/*` literal.
    qi::rule<itr_t> m_open_comment;

    //! `*/` literal.
    qi::rule<itr_t> m_close_comment;

    //! `-- ` literal.
    qi::rule<itr_t> m_dbl_dash;

    //! End of line literal.
    qi::rule<itr_t> m_eol;

    //! Any character sequence no /* or */.
    qi::rule<itr_t, std::string()> m_non_comment;

    /**
     * Simple comments parser with 2 productions.
     * 1. no embedded comments.
     * 2. embedded comments.
     */
    qi::rule<itr_t, std::string()> m_comments;
};

MysqlReplaceComments::MysqlReplaceComments() : ReplaceComments()
{
    build_parser();
}

MysqlReplaceComments::MysqlReplaceComments(
    const char *replacement
) :
    ReplaceComments(replacement)
{
    build_parser();
}

// To copy correctly we must re-build our parser.
MysqlReplaceComments::MysqlReplaceComments(const MysqlReplaceComments& that)
:
    ReplaceComments(that)
{
    build_parser();
}

void MysqlReplaceComments::build_parser()
{
    using boost::spirit::qi::labels::_val;

    // Basic symbols.
    m_open_comment  = qi::lit("/*");
    m_close_comment = qi::lit("*/");
    m_dbl_dash      = qi::lit("--") || qi::lit("#");
    m_eol           = qi::lit("\n");
    m_non_comment   = qi::char_ - m_close_comment;

    // When we find a comment, what do we do? Replace the comment or omit it?
    m_comments =
        qi::as_string[
            // Empty comment
            m_open_comment >> m_close_comment                 |

            // Comment that is not an embedded command.
            m_open_comment >> (qi::char_ - "!")
                           >> *(qi::char_ - m_close_comment)
                           >> m_close_comment                 |

            // End-of-line comment. Note: We keep the \n.
            m_dbl_dash >> *(qi::char_ - m_eol) >> m_eol [ _val += "\n" ]
        ][ _val += phoenix::ref(m_replacement) ];

    // Now setup our comments as part of a larger grammar with repetitions.
    m_parser = qi::as_string[
        qi::no_skip[
            *(qi::char_ - m_open_comment - m_dbl_dash) >> (m_comments % m_non_comment )
                                          >> *(qi::char_ - m_open_comment)
        ]
    ];
}

/**
 * Class to remove MySQL comments.
 *
 * This removes comments of the format:
 * - `-- comment` until the end of the line.
 * - `/+ /+ +/` style invalid comments (where the + is a * because of
 *    C++ commenting rules).
 */
class OracleReplaceComments : public ReplaceComments {
public:
    // Useful iterator type.
    typedef const char * itr_t;

    //! See ReplacementComments.
    OracleReplaceComments();

    //! See ReplacementComments.
    explicit OracleReplaceComments(const char *replacement);

    //! See ReplacementComments.
    OracleReplaceComments(const OracleReplaceComments& that);

private:
    //! Construct the parser objects. Called by all constructors.
    void build_parser();

    // Basic symbols.
    //! `/*` literal.
    qi::rule<itr_t> m_open_comment;

    //! `*/` literal.
    qi::rule<itr_t> m_close_comment;

    //! `-- ` literal.
    qi::rule<itr_t> m_dbl_dash;

    //! End of line literal.
    qi::rule<itr_t> m_eol;

    //! Any character sequence no /* or */.
    qi::rule<itr_t, std::string()> m_non_comment;

    /**
     * Simple comments parser with 2 productions.
     * 1. no embedded comments.
     * 2. embedded comments.
     */
    qi::rule<itr_t, std::string()> m_comments;
};

OracleReplaceComments::OracleReplaceComments() : ReplaceComments()
{
    build_parser();
}

OracleReplaceComments::OracleReplaceComments(
    const char *replacement
) :
    ReplaceComments(replacement)
{
    build_parser();
}

// To copy correctly we must re-build our parser.
OracleReplaceComments::OracleReplaceComments(const OracleReplaceComments& that)
:
    ReplaceComments(that)
{
    build_parser();
}

void OracleReplaceComments::build_parser()
{
    using boost::spirit::qi::labels::_val;

    // Basic symbols.
    m_open_comment  = qi::lit("/*");
    m_close_comment = qi::lit("*/");
    m_dbl_dash      = qi::lit("--") || qi::lit("#");
    m_eol           = qi::lit("\n");
    m_non_comment   = qi::char_ - m_close_comment;

    // When we find a comment, what do we do? Replace the comment or omit it?
    m_comments =
        qi::as_string[
            // Empty comment
            m_open_comment >> m_close_comment                 |

            // Comment that is not an embedded command.
            m_open_comment >> (qi::char_ - "!")
                           >> *(qi::char_ - m_close_comment)
                           >> m_close_comment                 |

            // End-of-line comment. Note: We keep the \n.
            m_dbl_dash >> *(qi::char_ - m_eol) >> m_eol [ _val += "\n" ]
        ][ _val += phoenix::ref(m_replacement) ];

    // Now setup our comments as part of a larger grammar with repetitions.
    m_parser = qi::as_string[
        qi::no_skip[
            *(qi::char_ - m_open_comment - m_dbl_dash) >> (m_comments % m_non_comment )
                                          >> *(qi::char_ - m_open_comment)
        ]
    ];
}

/**
 * Class to remove Block comments.
 *
 * This removes comments of the format:
 * - `/+ /+ +/` style invalid comments (where the + is a * because of
 *    C++ commenting rules).
 */
class NormalizeComments : public ReplaceComments {
public:
    // Useful iterator type.
    typedef const char * itr_t;

    //! See ReplacementComments.
    NormalizeComments();

    //! See ReplacementComments.
    NormalizeComments(const NormalizeComments& that);

private:
    //! Construct the parser objects. Called by all constructors.
    void build_parser();

    // Basic symbols.
    //! `/*` literal.
    qi::rule<itr_t> m_open_comment;

    //! `*/` literal.
    qi::rule<itr_t> m_close_comment;

    //! `-- ` literal.
    qi::rule<itr_t> m_dbl_dash;

    //! End of line literal.
    qi::rule<itr_t> m_eol;

    //! Any character sequence no /* or */.
    qi::rule<itr_t, std::string()> m_non_comment;

    /**
     * Simple comments parser with 2 productions.
     * 1. no embedded comments.
     * 2. embedded comments.
     */
    qi::rule<itr_t, std::string()> m_comments;
};

NormalizeComments::NormalizeComments() : ReplaceComments()
{
    build_parser();
}

// To copy correctly we must re-build our parser.
NormalizeComments::NormalizeComments(const NormalizeComments& that)
:
    ReplaceComments(that)
{
    build_parser();
}

void NormalizeComments::build_parser()
{
    using boost::spirit::qi::labels::_val;
    using boost::spirit::qi::labels::_1;

    // Basic symbols.
    m_open_comment  = qi::lit("/*");
    m_close_comment = qi::lit("*/");
    m_non_comment   = qi::char_ - m_close_comment;

    // When we find a comment, what do we do? Replace the comment or omit it?
    m_comments =
        qi::as_string[
            // Empty comment
            m_open_comment >> m_close_comment                 |

            // Comment that is not an embedded command.
            m_open_comment >> (qi::char_ - "!")
                           >> *(qi::char_ - m_close_comment)
                           >> m_close_comment                 |

            // Executing comment.
            // Remove comment characters and leave body portion.
            (m_open_comment >> qi::lit("!"))
                            >> (-qi::ulong_long[ _val += "" ])
                            >> *(qi::char_ - m_close_comment)[_val += _1 ]
                            >> m_close_comment
        ];

    // Now setup our comments as part of a larger grammar with repetitions.
    m_parser = qi::as_string[
        qi::no_skip[
            *(qi::char_ - m_open_comment - m_dbl_dash) >> (m_comments % m_non_comment )
                                          >> *(qi::char_ - m_open_comment)
        ]
    ];
}

/**
 * Transformation generator that instantiates a PgReplaceComments object.
 *
 * @param[in] mm The memory manager. Unused.
 * @param[in] replacement The text used to replace comments.
 *
 * @returns a transformation instance object that will remove postgres comments.
 */
Transformation::transformation_instance_t replace_pg_comments_tfn_generator(
    MemoryManager mm,
    const char *replacement
)
{
    assert(replacement != NULL);

    return PgReplaceComments(replacement);
}

/**
 * Transformation generator that instantiates a MysqlReplaceComments object.
 *
 * @param[in] mm The memory manager. Unused.
 * @param[in] replacement The text used to replace comments.
 *
 * @returns a transformation instance object that will remove postgres comments.
 */
Transformation::transformation_instance_t replace_mysql_comments_tfn_generator(
    MemoryManager mm,
    const char *replacement
)
{
    assert(replacement != NULL);

    return MysqlReplaceComments(replacement);
}

/**
 * Transformation generator that instantiates an OracleReplaceComments object.
 *
 * @param[in] mm The memory manager. Unused.
 * @param[in] replacement The text used to replace comments.
 *
 * @returns a transformation instance object that will remove postgres comments.
 */
Transformation::transformation_instance_t replace_oracle_comments_tfn_generator(
    MemoryManager mm,
    const char *replacement
)
{
    assert(replacement != NULL);

    return OracleReplaceComments(replacement);
}

/**
 * Transformation generator that instantiates an NormalizeComments object.
 *
 * @param[in] mm The memory manager. Unused.
 * @param[in] replacement The text used to replace comments.
 *
 * @returns a transformation instance object that will normalize comments.
 */
Transformation::transformation_instance_t sql_comments_normalize_tfn_generator(
    MemoryManager mm,
    const char *replacement
)
{
    assert(replacement != NULL);

    return NormalizeComments();
}

class SqlModuleDelegate : public ModuleDelegate {
public:
    /**
     * Constructor.
     *
     * @param[in] module The module.
     */
    explicit SqlModuleDelegate(Module m);

};

} // Anonymous Namespace

SqlModuleDelegate::SqlModuleDelegate(Module m) : ModuleDelegate(m)
{
    Transformation::create(
        m.engine().main_memory_mm(),
        "replace_pg_comments",
        false,
        replace_pg_comments_tfn_generator
    ).register_with(m.engine());

    Transformation::create(
        m.engine().main_memory_mm(),
        "replace_mysql_comments",
        false,
        replace_mysql_comments_tfn_generator
    ).register_with(m.engine());

    Transformation::create(
        m.engine().main_memory_mm(),
        "replace_oracle_comments",
        false,
        replace_oracle_comments_tfn_generator
    ).register_with(m.engine());

    // Currently the MySQL comment code seems a good "general" implementation.
    Transformation::create(
        m.engine().main_memory_mm(),
        "replace_sql_comments",
        false,
        replace_mysql_comments_tfn_generator
    ).register_with(m.engine());

    Transformation::create(
        m.engine().main_memory_mm(),
        "normalize_sql_comments",
        false,
        sql_comments_normalize_tfn_generator
    ).register_with(m.engine());
}

IBPP_BOOTSTRAP_MODULE_DELEGATE("sql", SqlModuleDelegate);
