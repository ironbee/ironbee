dnl Check for compiler security features and optimizations 
dnl GCC_CHARACTERISTICS(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  GCC_CHARACTERISTICS_CFLAGS
dnl  GCC_CHARACTERISTICS_LDFLAGS

GCC_CHARACTERISTICS_CFLAGS=""
GCC_CHARACTERISTICS_CPPFLAGS=""
GCC_CHARACTERISTICS_CXXFLAGS=""
GCC_CHARACTERISTICS_LDFLAGS=""
GCC_FSTACK_FLAGS=""
GCC_CPP_STD_SET=""
GCC_C_STD_SET=""

AC_DEFUN([GCC_CHARACTERISTICS],
[dnl
dnl -----------------------------------------------
dnl Check and enable the GCC opts we want to use.
dnl We may need to add more checks
dnl
dnl http://www.linuxfromscratch.org/hints/downloads/files/ssp.txt
dnl https://wiki.ubuntu.com/CompilerFlags
dnl http://en.wikipedia.org/wiki/Buffer_overflow_protection
dnl http://people.redhat.com/drepper/nonselsec.pdf
dnl http://gcc.gnu.org/ml/gcc-patches/2004-09/msg02055.html
dnl http://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
dnl -----------------------------------------------

dnl -----------------------------------------------
dnl TODO: Convert existing CPPFLAGS to CFLAGS/CXXFLAGS
dnl TODO: Create tests for CXX only flags
dnl -----------------------------------------------

dnl -----------------------------------------------
dnl Check for GCC Optimization level
dnl -----------------------------------------------
AC_ARG_WITH(gcc_optimization_level,
            AS_HELP_STRING([--with-gcc-optimization-level],[(0|1|2|3|s|fast) See the following url for explanation of levels http://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html. Defaults to 2]),
            [ gcc_optimization_level="${with_gcc_optimization_level}"]
            ,[ gcc_optimization_level="2"])

    if test "$gcc_optimization_level"="0" || test "$gcc_optimization_level"="1" || test "$gcc_optimization_level"="2" \
       || test "$gcc_optimization_level"="3" ||  test "$gcc_optimization_level"="s" || test "$gcc_optimization_level"="fast"; then
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"
        
        AC_MSG_CHECKING(gcc optimization level for C)
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -g -O${gcc_optimization_level}"
        AC_TRY_COMPILE(,,[gcc_have_optimization_level=yes],[gcc_have_optimization_level=no])
        if test "$gcc_have_optimization_level" = "yes"; then
            AC_MSG_RESULT(-g -O${gcc_optimization_level})
            GCC_CHARACTERISTICS_CFLAGS="-g -O${gcc_optimization_level}"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   The optimization level of ${gcc_optimization_level}}"
            echo "   appears be unsupported by your C compiler."
            echo
        fi

        AC_MSG_CHECKING(gcc optimization level for C++)
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -g -O${gcc_optimization_level}"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_optimization_level=yes],[gcc_have_optimization_level=no])
        if test "$gcc_have_optimization_level" = "yes"; then
            AC_MSG_RESULT(-g -O${gcc_optimization_level})
            GCC_CHARACTERISTICS_CXXFLAGS="-g -O${gcc_optimization_level}"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   The optimization level of ${gcc_optimization_level}}"
            echo "   appears be unsupported by your C compiler."
            echo
        fi
        AC_LANG_RESTORE
    else
        echo
        echo "   The optimization level of ${gcc_optimization_level}"
        echo "   is either invalid (0|1|2|3|s|fast)  or is not supported by your compiler. "
        echo "   http://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html"
        echo
    fi

    CFLAGS="${save_CFLAGS}"
    CXXFLAGS="${save_CXXFLAGS}"
dnl -----------------------------------------------
dnl Check for GCC -Wall support 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_all,
              AS_HELP_STRING([--disable-gcc-warn-all], [Disable GCC warnings about constructions that some users consider questionable]))

    AC_MSG_CHECKING(gcc -Wall support)
    if test "$enable_gcc_warn_all" != "no"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -Wall"
        AC_TRY_COMPILE(,,[gcc_have_warn_all=yes],[gcc_have_warn_all=no])

        if test "$gcc_have_warn_all" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -Wall"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wall should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC -W or -Wextra support 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_extra,
              AS_HELP_STRING([--disable-gcc-warn-extra], [Disable GCC some extra warning flags set by -Wextra or -W that are not enabled by -Wall]))

    AC_MSG_CHECKING(gcc -Wextra,-W)
    if test "$enable_gcc_warn_extra" != "no"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -W"
        AC_TRY_COMPILE(,,[gcc_have_warn_extra=yes],[gcc_have_warn_extra=no])

        if test "$gcc_have_warn_extra" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -W"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -W should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi
dnl -----------------------------------------------
dnl Check for GCC -Wmissing-field-initializers 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_missing_field_initializers,
              AS_HELP_STRING([--enable-gcc-warn-missing-field-initializers], [Enable GCC warnings if a structure's initializer has some fields missing]))

    AC_MSG_CHECKING(gcc -Wmissing-field-initializers)
    if test "$enable_gcc_warn_missing_field_initializers" = "yes"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -Wmissing-field-initializers"
        AC_TRY_COMPILE(,,[gcc_have_warn_missing_field_initializers=yes],[gcc_have_warn_missing_field_initializers=no])

        if test "$gcc_have_warn_missing_field_initializers" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -Wmissing-field-initializers"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wmissing-field-initializers should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
        AC_MSG_RESULT(no)
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -Wno-missing-field-initializers"
        AC_TRY_COMPILE(,,[gcc_have_warn_missing_field_initializers=yes],[gcc_have_warn_missing_field_initializers=no])

        if test "$gcc_have_warn_missing_field_initializers" = "yes"; then
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -Wno-missing-field-initializers"
        fi
        CPPFLAGS="${save_CPPFLAGS}" 
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wstrict-prototypes (C and Objective C only)
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_strict_prototypes,
              AS_HELP_STRING([--disable-gcc-warn-strict-prototypes], [Disable GCC warnings if a function is declared or defined without specifying the argument types.]))

    AC_MSG_CHECKING(gcc strict prototype warnings)
    if test "$enable_gcc_warn_strict_prototypes" != "no"; then
        save_CFLAGS="${CFLAGS}"
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wstrict-prototypes"
        AC_TRY_COMPILE(,,[gcc_have_warn_strict_prototypes=yes],[gcc_have_warn_strict_prototypes=no])

        if test "$gcc_have_warn_strict_prototypes" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wstrict-prototypes"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wstrict-prototypes should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CFLAGS="${save_CFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wmissing-prototypes (C and Objective C only)
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_missing_prototypes,
              AS_HELP_STRING([--disable-gcc-warn-missing-prototypes], [Disable GCC warnings if a global function is defined without a previous prototype declaration]))

    AC_MSG_CHECKING(gcc missing prototype warnings)
    if test "$enable_gcc_warn_missing_prototypes" != "no"; then
        save_CFLAGS="${CFLAGS}"
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wmissing-prototypes"
        AC_TRY_COMPILE(,,[gcc_have_warn_missing_prototypes=yes],[gcc_have_warn_missing_prototypes=no])

        if test "$gcc_have_warn_missing_prototypes" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wmissing-prototypes"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wmissing-prototypes should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CFLAGS="${save_CFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wbad-function-cast (C and Objective C only)
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_bad_function_cast,
              AS_HELP_STRING([--disable-gcc-warn-bad-function-cast], [Disable GCC warnings whenever a function call is cast to a non-matching type.]))

    AC_MSG_CHECKING(gcc bad function cast warnings)
    if test "$enable_gcc_warn_bad_function_cast" != "no"; then
        save_CFLAGS="${CFLAGS}"
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wbad-function-cast"
        AC_TRY_COMPILE(,,[gcc_have_warn_bad_function_cast=yes],[gcc_have_warn_bad_function_cast=no])

        if test "$gcc_have_warn_bad_function_cast" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wbad-function-cast"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wbad-function-cast should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CFLAGS="${save_CFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wnested-externs (C and Objective C only)
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_nested_externs,
              AS_HELP_STRING([--disable-gcc-warn-nested-externs], [Disable GCC warnings if an extern declaration is encountered within a function.]))

    AC_MSG_CHECKING(gcc nested externs)
    if test "$enable_gcc_warn_nested_externs" != "no"; then
        save_CFLAGS="${CFLAGS}"
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wnested-externs"
        AC_TRY_COMPILE(,,[gcc_have_warn_nested_externs=yes],[gcc_have_warn_nested_externs=no])

        if test "$gcc_have_warn_nested_externs" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wnested-externs"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wnested-externs should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CFLAGS="${save_CFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wunused-parameter 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_unused_parameter,
              AS_HELP_STRING([--enable-gcc-warn-unused-parameter], [Enable GCC warnings whenever a function parameter is unused aside from its declaration]))

    if test "$enable_gcc_warn_unused_parameter" = "yes"; then
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"

        AC_MSG_CHECKING(gcc C -Wunused-parameter support)
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wunused-parameter"
        AC_TRY_COMPILE(,,[gcc_have_c_warn_unused_parameter=yes],[gcc_have_c_warn_unused_parameter=no])
        if test "$gcc_have_c_warn_unused_parameter" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wunused-parameter"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wunused-parameter should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi

        AC_MSG_CHECKING(gcc C++ -Wunused-parameter support)
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -Wunused-parameter"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_cpp_warn_unused_parameter=yes],[gcc_have_cpp_warn_unused_parameter=no])
        if test "$gcc_have_cpp_warn_unused_parameter" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -Wunused-parameter"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wunused-parameter should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        AC_LANG_RESTORE

        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
    else
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wno-unused-parameter"
        AC_TRY_COMPILE(,,[gcc_have_c_warn_unused_parameter=yes],[gcc_have_c_warn_unused_parameter=no])
        if test "$gcc_have_c_warn_unused_parameter" = "yes"; then
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wno-unused-parameter"
        fi


        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -Wno-unused-parameter"
        AC_TRY_COMPILE(,,[gcc_have_cpp_warn_unused_parameter=yes],[gcc_have_cpp_warn_unused_parameter=no])
        if test "$gcc_have_cpp_warn_unused_parameter" = "yes"; then
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -Wno-unused-parameter"
        fi
        AC_LANG_RESTORE
        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wformat-nonliteral 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_format_nonliteral,
              AS_HELP_STRING([--disable-gcc-warn-format-nonliteral], [Disable GCC warnings if the format string is not a string literal and so cannot be checked]))

    if test "$enable_gcc_warn_format_nonliteral" != "no"; then
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"

        AC_MSG_CHECKING(gcc C -Wformat -Wformat-nonliteral support)
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wformat -Wformat-nonliteral"
        AC_TRY_COMPILE(,,[gcc_have_c_warn_format_nonliteral=yes],[gcc_have_c_warn_format_nonliteral=no])
        if test "$gcc_have_c_warn_format_nonliteral" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wformat -Wformat-nonliteral"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wformat -Wformat-nonliteral  should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi

        AC_MSG_CHECKING(gcc C++ -Wformat -Wformat-nonliteral support)
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -Wformat -Wformat-nonliteral"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_cpp_warn_format_nonliteral=yes],[gcc_have_cpp_warn_format_nonliteral=no])
        if test "$gcc_have_cpp_warn_format_nonliteral" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -Wformat -Wformat-nonliteral"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wformat -Wformat-nonliteral  should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        AC_LANG_RESTORE

        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wcast-align 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_cast_align,
              AS_HELP_STRING([--disable-gcc-warn-cast-align], [Disable GCC warnings Warn whenever a pointer is cast such that the required alignment of the target is increased]))

    if test "$enable_gcc_warn_cast_align" != "no"; then
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"

        AC_MSG_CHECKING(gcc C -Wcast-align support)
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wcast-align"
        AC_TRY_COMPILE(,,[gcc_have_c_warn_cast_align=yes],[gcc_have_c_warn_cast_align=no])
        if test "$gcc_have_c_warn_cast_align" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wcast-align"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wcast-align  should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi

        AC_MSG_CHECKING(gcc C++ -Wcast-align support)
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -Wcast-align"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_cpp_warn_cast_align=yes],[gcc_have_cpp_warn_cast_align=no])
        if test "$gcc_have_cpp_warn_cast_align" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -Wcast-align"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wcast-align  should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        AC_LANG_RESTORE

        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wmissing-declarations 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_missing_declarations,
              AS_HELP_STRING([--disable-gcc-warn-missing-declarations], [Disable GCC warnings if a global function is defined without a previous declaration.]))

    if test "$enable_gcc_warn_missing_declarations" != "no"; then
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"

        AC_MSG_CHECKING(gcc C -Wmissing-declarations support)
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wmissing-declarations"
        AC_TRY_COMPILE(,,[gcc_have_c_warn_missing_declarations=yes],[gcc_have_c_warn_missing_declarations=no])
        if test "$gcc_have_c_warn_missing_declarations" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wmissing-declarations"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wmissing-declarations should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi

        AC_MSG_CHECKING(gcc C++ -Wmissing-declarations support)
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -Wmissing-declarations"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_cpp_warn_missing_declarations=yes],[gcc_have_cpp_warn_missing_declarations=no])
        if test "$gcc_have_cpp_warn_missing_declarations" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -Wmissing-declarations"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wmissing-declarations should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        AC_LANG_RESTORE

        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
    fi

dnl -----------------------------------------------
dnl Check for GCC -Winline 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_inline,
              AS_HELP_STRING([--disable-gcc-warn-inline], [Disable GCC warnings if a function can not be inlined and it was declared as inline.]))

    if test "$enable_gcc_warn_inline" != "no"; then
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"

        AC_MSG_CHECKING(gcc C -Winline support)
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Winline"
        AC_TRY_COMPILE(,,[gcc_have_c_warn_inline=yes],[gcc_have_c_warn_inline=no])
        if test "$gcc_have_c_warn_inline" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Winline"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Winline should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi

        AC_MSG_CHECKING(gcc C++ -Winline support)
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -Winline"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_cpp_warn_inline=yes],[gcc_have_cpp_warn_inline=no])
        if test "$gcc_have_cpp_warn_inline" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -Winline"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Winline should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        AC_LANG_RESTORE

        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wundef 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_undef,
              AS_HELP_STRING([--disable-gcc-warn-undef], [Disable GCC warnings Warn if an undefined identifier is evaluated in an #if directive. ]))

    if test "$enable_gcc_warn_undef" != "no"; then
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"

        AC_MSG_CHECKING(gcc C -Wundef support)
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wundef"
        AC_TRY_COMPILE(,,[gcc_have_c_warn_undef=yes],[gcc_have_c_warn_undef=no])
        if test "$gcc_have_c_warn_undef" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wundef"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wundef should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi

        AC_MSG_CHECKING(gcc C++ -Wundef support)
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -Wundef"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_cpp_warn_undef=yes],[gcc_have_cpp_warn_undef=no])
        if test "$gcc_have_cpp_warn_undef" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -Wundef"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wundef should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        AC_LANG_RESTORE

        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wconversion 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_conversion,
              AS_HELP_STRING([--enable-gcc-warn-conversion], [Enable GCC warnings for implicit conversions that may alter a value.]))

    if test "$enable_gcc_warn_conversion" = "yes"; then
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"

        AC_MSG_CHECKING(gcc C -Wconversion support)
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wconversion"
        AC_TRY_COMPILE(,,[gcc_have_c_warn_conversion=yes],[gcc_have_c_warn_conversion=no])
        if test "$gcc_have_c_warn_conversion" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wconversion"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wconversion should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi

        AC_MSG_CHECKING(gcc C++ -Wconversion support)
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -Wconversion"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_cpp_warn_conversion=yes],[gcc_have_cpp_warn_conversion=no])
        if test "$gcc_have_cpp_warn_conversion" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -Wconversion"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wconversion should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        AC_LANG_RESTORE

        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wwrite-strings 
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_write_strings,
              AS_HELP_STRING([--disable-gcc-warn-write-strings], [Disable GCC warnings that will help you find at compile time code that can try to write into a string constant]))

    if test "$enable_gcc_warn_write_strings" != "no"; then
        save_CFLAGS="${CFLAGS}"
        save_CXXFLAGS="${CXXFLAGS}"

        AC_MSG_CHECKING(gcc C -Wwrite-strings support)
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -Wwrite-strings"
        AC_TRY_COMPILE(,,[gcc_have_c_warn_write_strings=yes],[gcc_have_c_warn_write_strings=no])
        if test "$gcc_have_c_warn_write_strings" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -Wwrite-strings"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wwrite-strings should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi

        AC_MSG_CHECKING(gcc C++ -Wwrite-strings  support)
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -Wwrite-strings"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_cpp_warn_write_strings=yes],[gcc_have_cpp_warn_write_strings=no])
        if test "$gcc_have_cpp_warn_write_strings" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -Wwrite-strings"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wwrite-strings should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        AC_LANG_RESTORE

        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wshadow support
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_shadow,
              AS_HELP_STRING([--disable-gcc-warn-shadow], [Disable GCC Warnings whenever a local variable shadows another local variable, parameter or global variable or whenever a built-in function is shadowed]))

    AC_MSG_CHECKING(gcc -Wshadow support)
    if test "$enable_gcc_warn_shadow" != "no"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -Wshadow"
        AC_TRY_COMPILE(,,[gcc_have_warn_shadow=yes],[gcc_have_warn_shadow=no])

        if test "$gcc_have_warn_shadow" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -Wshadow"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wshadow should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC Wpointer-arith support
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_pointer_arith,
              AS_HELP_STRING([--disable-gcc-warn-pointer-arith], [Disable GCC warnings about anything that depends on the size of a function type or of void]))

    AC_MSG_CHECKING(gcc -Wpointer-arith support)
    if test "$enable_gcc_warn_pointer_arith" != "no"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -Wpointer-arith"
        AC_TRY_COMPILE(,,[gcc_have_warn_pointer_arith=yes],[gcc_have_warn_pointer_arith=no])

        if test "$gcc_have_warn_pointer_arith" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -Wpointer-arith"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wpointer-arith should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC -Wformat-security support
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_format_security,
              AS_HELP_STRING([--disable-gcc-warn-format-security], [Disable GCC format string misuse warnings]))

    AC_MSG_CHECKING(gcc format string warning support)
    if test "$enable_gcc_warn_format_security" != "no"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -Wformat -Wformat-security"
        AC_TRY_COMPILE(,,[gcc_have_warn_format_security=yes],[gcc_have_warn_format_security=no])

        if test "$gcc_have_warn_format_security" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -Wformat -Wformat-security"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Wformat -Wformat-security should be set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -Wno-format-security"
        AC_TRY_COMPILE(,,[gcc_have_warn_format_security=yes],[gcc_have_warn_format_security=no])

        if test "$gcc_have_warn_format_security" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -Wno-format-security"
        else
            AC_MSG_RESULT(no)
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    fi

dnl -----------------------------------------------
dnl Check for GCC signed overflow warning support
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_strict_overflow,
              AS_HELP_STRING([--disable-gcc-warn-strict-overflow], [Disable GCC signed overflow warning support]))

    AC_MSG_CHECKING(gcc signed overflow support)    
    if test "$enable_gcc_warn_strict_overflow" != "no"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -Wstrict-overflow=1"
        AC_TRY_COMPILE(,,[gcc_have_strict_overflow=yes],[gcc_have_strict_overflow=no])

        if test "$gcc_have_strict_overflow" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -Wstrict-overflow=1"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified --enable-gcc-warn-strict-overflow should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC -Werror
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_as_error,
              AS_HELP_STRING([--enable-gcc-warn-as-error], [Enable GCC warnings to be treated like errors]))

    AC_MSG_CHECKING(gcc -Werror support)
    if test "$enable_gcc_warn_as_error" = "yes"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -Werror"
        AC_TRY_COMPILE(,,[gcc_have_warn_as_error=yes],[gcc_have_warn_as_error=no])

        if test "$gcc_have_warn_as_error" = "yes"; then
            AC_MSG_RESULT(yes)
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -Werror"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -Werror should set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC stack smashing protection
dnl -----------------------------------------------
AC_ARG_WITH(gcc_fstack_protection,
             AS_HELP_STRING([--with-gcc-fstack-protection],[(default|all|none) GCC SSP for char arrays(default) or for all data types(all), or don't enable protection]),
            [ gcc_fstack_protection="${with_gcc_fstack_protection}"],
            [ gcc_fstack_protection="default"])

    AC_MSG_CHECKING(gcc stack smashing protection)
    if test "$gcc_fstack_protection" = "default"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -fstack-protector"
        AC_TRY_COMPILE(,,[gcc_have_fstack_protection=yes],[gcc_have_fstack_protection=no])

        if test "$gcc_have_fstack_protection" = "yes"; then
            AC_MSG_RESULT(-fstack-protector)
            GCC_FSTACK_FLAGS="-fstack-protector"
        else
                AC_MSG_RESULT(no)
                echo
                echo "   You specified --fstack-protector should be set but it was not added"
                echo "   as it appears to be unsupported by your compiler."
                echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    elif test "$gcc_fstack_protection" = "all"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -fstack-protector-all"
        AC_TRY_COMPILE(,,[gcc_have_fstack_protection=yes],[gcc_have_fstack_protection=no])

        if test "$gcc_have_fstack_protection" = "yes"; then
            AC_MSG_RESULT(-fstack-protector-all)
            GCC_FSTACK_FLAGS="-fstack-protector-all"
        else
                echo
                echo "   You specified --fstack-protector-all should be set but it was not added"
                echo "   as it appears to be unsupported by your compiler."
                echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    elif test "$gcc_fstack_protection" = "none"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -fno-stack-protector"
        AC_TRY_COMPILE(,,[gcc_have_fstack_protection=yes],[gcc_have_fstack_protection=no])

        if test "$gcc_have_fstack_protection" = "yes"; then
            AC_MSG_RESULT(-fno-stack-protector)
            GCC_FSTACK_FLAGS="-fno-stack-protector"
        else
                AC_MSG_RESULT(no)
                echo
                echo "   You specified -fno-stack-protector should be set but it was not added"
                echo "   as it appears to be unsupported by your compiler."
                echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
                AC_MSG_RESULT(no)
                echo
                echo "   you specified an invalid option $gcc_fstack_protection to --with-gcc-fstack-protection."
                echo "   valid options are default, all, and none, or don't specify the option at all."
                echo
    fi

AC_ARG_WITH(gcc_fstack_buffer_size,
            AS_HELP_STRING([--with-gcc-fstack-buffer-size],[(int|none) minimum buffer size protected by -fstack-protector by setting the user provided value to --param ssp-buffer-size]),
            [ gcc_fstack_buffer_size="${with_gcc_fstack_buffer_size}"]
            ,[ gcc_fstack_buffer_size="4"])
    AC_MSG_CHECKING(gcc user specified fstack mimimum buffer size)
    if test "$gcc_fstack_buffer_size" != "none"; then
        if test -n "$GCC_FSTACK_FLAGS" && test "$GCC_FSTACK_FLAGS" != "-fno-stack-protector"; then
            save_CPPFLAGS="${CPPFLAGS}"
            CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} ${GCC_FSTACK_FLAGS} --param ssp-buffer-size=${gcc_fstack_buffer_size}"
            AC_TRY_COMPILE(,,[gcc_have_fstack_buffer_size=yes],[gcc_have_fstack_buffer_size=no])

            if test "$gcc_have_fstack_buffer_size" = "yes"; then
                AC_MSG_RESULT(${gcc_fstack_buffer_size})
                GCC_FSTACK_FLAGS="${GCC_FSTACK_FLAGS} --param ssp-buffer-size=${gcc_fstack_buffer_size}"
            else
                echo
                echo "   You specified --param ssp-buffer-size=${gcc_fstack_buffer_size} should be set but it was not added"
                echo "   as it appears to be unsupported by your compiler."
                echo
            fi
            CPPFLAGS="${save_CPPFLAGS}"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified --param ssp-buffer-size=${gcc_fstack_buffer_size} should be set but it was not added"
            echo "   as you have not specified a valid --with-gcc-fstack-protection option or "
            echo "   --fstack-protector is not supported by your compiler."
            echo
        fi
    else
         AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Add the final fstack protection flags back in
dnl -----------------------------------------------
if test -n "$GCC_FSTACK_FLAGS"; then
    GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} ${GCC_FSTACK_FLAGS}"
fi

dnl -----------------------------------------------
dnl Check for GCC -D_FORTIFY_SOURCE support
dnl -----------------------------------------------
AC_ARG_WITH(gcc_fortify_source,
            AS_HELP_STRING([--with-gcc-fortify-source],[(0|1|2) 0 disabled, 1 some protection, 2 maximum protection(default)]),
            [ gcc_fortify_source="${with_gcc_fortify_source}"]
            ,[ gcc_fortify_source="2"])
    AC_MSG_CHECKING(gcc fortify source level)
    if test "$gcc_fortify_source" -ge "0" && test "$gcc_fortify_source" -le "2"; then
        save_CPPFLAGS="${CPPFLAGS}"
        CPPFLAGS="${CPPFLAGS} ${GCC_CHARACTERISTICS_CPPFLAGS} -D_FORTIFY_SOURCE=${gcc_fortify_source}"
        AC_TRY_COMPILE(,,[gcc_have_fortify_source=yes],[gcc_have_foritfy_source=no])
        if test "$gcc_have_fortify_source" = "yes"; then
            AC_MSG_RESULT(${gcc_fortify_source})
            GCC_CHARACTERISTICS_CPPFLAGS="${GCC_CHARACTERISTICS_CPPFLAGS} -D_FORTIFY_SOURCE=${gcc_fortify_source}"
        else
            echo
            echo "   The value provided to -D_FORTIFY_SOURCE of ${gcc_fortify_source}"
            echo "   appears be unsupported by your compiler."
            echo
        fi
        CPPFLAGS="${save_CPPFLAGS}"
    else
        AC_MSG_RESULT(no)
        echo
        echo "   The value provided to -D_FORTIFY_SOURCE of ${gcc_fortify_source}"
        echo "   is either outside of the valid range 0-2 or is not supported by your compiler. "
        echo "   http://gcc.gnu.org/ml/gcc-patches/2004-09/msg02055.html"
        echo
    fi

dnl -----------------------------------------------
dnl GCC Check for relocation support
dnl -----------------------------------------------
AC_ARG_WITH(gcc_relocation,
            AS_HELP_STRING([--with-gcc-relocation],[(none,norelo,nonow,relo,now) none disabled, relo Provides a read-only relocation table area (default), adds relo support but also adds -z now forces resolution of all dynamic symbols at start-up]),
            [ gcc_relocation="${with_gcc_relocation}"]
            ,[ gcc_relocation="relo"])

    case "$gcc_relocation" in
        none)
            AC_MSG_CHECKING(gcc relocation setting)
            save_LDFLAGS="${LDFLAGS}"
            LDFLAGS="${LDFLAGS} ${GCC_CHARACTERISTICS_LDFLAGS} -z norelo -z nonow"
            AC_TRY_LINK(,,[gcc_have_norelo_both=yes],[gcc_have_norelo_both=no])
            if test "$gcc_have_norelo_both" = "yes"; then
                AC_MSG_RESULT(-z norelo -z nonow)
                GCC_CHARACTERISTICS_LDFLAGS="${GCC_CHARACTERISTICS_LDFLAGS} -z norelo -z nonow"
            else
                AC_MSG_RESULT(unsupported)
                echo
                echo "   The value provided to LDFLAGS of -z norelo -z nonow"
                echo "   appears be unsupported by your compiler."
                echo
            fi
            LDFLAGS="${save_LDFLAGS}"
            ;;
        norelo)
            AC_MSG_CHECKING(gcc relocation setting)
            save_LDFLAGS="${LDFLAGS}"
            LDFLAGS="${LDFLAGS} ${GCC_CHARACTERISTICS_LDFLAGS} -z norelo"
            AC_TRY_LINK(,,[gcc_have_norelo=yes],[gcc_have_norelo=no])
            if test "$gcc_have_norelo_both" = "yes"; then
                AC_MSG_RESULT(-z norelo)
                GCC_CHARACTERISTICS_LDFLAGS="${GCC_CHARACTERISTICS_LDFLAGS} -z norelo"
            else
                AC_MSG_RESULT(unsupported)
                echo
                echo "   The value provided to LDFLAGS of -z norelo"
                echo "   appears be unsupported by your compiler."
                echo
            fi
            LDFLAGS="${save_LDFLAGS}"
            ;;
        nonow)
            AC_MSG_CHECKING(gcc relocation setting)
            save_LDFLAGS="${LDFLAGS}"
            LDFLAGS="${LDFLAGS} ${GCC_CHARACTERISTICS_LDFLAGS} -z nonow"
            AC_TRY_LINK(,,[gcc_have_norelo_both=yes],[gcc_have_norelo_both=no])
            if test "$gcc_have_norelo_both" = "yes"; then
                AC_MSG_RESULT(-z nonow)
                GCC_CHARACTERISTICS_LDFLAGS="${GCC_CHARACTERISTICS_LDFLAGS} -z nonow"
            else
                AC_MSG_RESULT(unsupported)
                echo
                echo "   The value provided to LDFLAGS of -z nonow"
                echo "   appears be unsupported by your compiler."
                echo
            fi
            LDFLAGS="${save_LDFLAGS}"
            ;;
        relo)
            AC_MSG_CHECKING(gcc relocation setting)
            save_LDFLAGS="${LDFLAGS}"
            LDFLAGS="${LDFLAGS} ${GCC_CHARACTERISTICS_LDFLAGS} -z relo"
            AC_TRY_LINK(,,[gcc_have_relo=yes],[gcc_have_relo=no])
            if test "$gcc_have_relo" = "yes"; then
                AC_MSG_RESULT(-z relo)
                GCC_CHARACTERISTICS_LDFLAGS="${GCC_CHARACTERISTICS_LDFLAGS} -z relo"
            else
                AC_MSG_RESULT(unsupported)
                echo
                echo "   The value provided to LDFLAGS of -z relo"
                echo "   appears be unsupported by your compiler."
                echo
            fi
            LDFLAGS="${save_LDFLAGS}"
            ;;
        now)
            AC_MSG_CHECKING(gcc relocation setting)
            save_LDFLAGS="${LDFLAGS}"
            LDFLAGS="${LDFLAGS} ${GCC_CHARACTERISTICS_LDFLAGS} -z relo -z now"
            AC_TRY_LINK(,,[gcc_have_relo=yes],[gcc_have_relo=no])
            if test "$gcc_have_relo" = "yes"; then
                AC_MSG_RESULT(-z relo -z now)
                GCC_CHARACTERISTICS_LDFLAGS="${GCC_CHARACTERISTICS_LDFLAGS} -z relo -z now"
            else
                AC_MSG_RESULT(unsupported)
                echo
                echo "   The value provided to LDFLAGS of -z norelo -z nonow"
                echo "   appears be unsupported by your compiler."
                echo
            fi
            LDFLAGS="${save_LDFLAGS}"
            ;;
        *)
        AC_MSG_WARN([unsupported relo setting not adding])
        ;;
    esac

dnl -----------------------------------------------
dnl Check for GCC C language standard support
dnl -----------------------------------------------
AC_ARG_WITH(gcc_c_standard,
            AS_HELP_STRING([--with-gcc-c-standard],[(C language standard|none) value to pass to the -std flag (default is c99) man gcc for other options]),
            [ gcc_c_standard="${with_gcc_c_standard}"]
            ,[ gcc_c_standard="c99"])
    AC_MSG_CHECKING(gcc C language standard support)
    if test -n "$gcc_c_standard"; then
        save_CFLAGS="${CFLAGS}"
        CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -std=${gcc_c_standard}"
        AC_TRY_COMPILE(,,[gcc_have_c_standard=yes],[gcc_have_c_standard=no])
        if test "$gcc_have_c_standard" = "yes"; then
            AC_MSG_RESULT(${gcc_c_standard})
            GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -std=${gcc_c_standard}"
            GCC_C_STD_SET="yes"
        else
            echo
            echo "   The value provided -std of ${gcc_c_standard}"
            echo "   appears be unsupported by your compiler."
            echo
        fi
        CFLAGS="${save_CFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC C++ language standard support
dnl -----------------------------------------------
AC_ARG_WITH(gcc_cpp_standard,
            AS_HELP_STRING([--with-gcc-cpp-standard],[(C++ language standard|none) value to pass to the -std flag (default is c++0x) man gcc for other options]),
            [ gcc_cpp_standard="${with_gcc_cpp_standard}"]
            ,[ gcc_cpp_standard="c++0x"])
    AC_MSG_CHECKING(gcc C++ language standard support)
    if test "$gcc_cpp_standard" != "none"; then
        save_CXXFLAGS="${CXXFLAGS}"
        CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -std=${gcc_cpp_standard}"
        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        AC_TRY_COMPILE(,,[gcc_have_cpp_standard=yes],[gcc_have_cpp_standard=no])
        if test "$gcc_have_cpp_standard" = "yes"; then
            AC_MSG_RESULT(${gcc_cpp_standard})
            GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -std=${gcc_cpp_standard}"
            GCC_CPP_STD_SET="yes"
        else
            echo
            echo "   The value provided -std of ${gcc_cpp_standard}"
            echo "   appears be unsupported by your compiler."
            echo
        fi
        AC_LANG_RESTORE
        CXXFLAGS="${save_CXXFLAGS}"
    else
        AC_MSG_RESULT(no)
    fi

dnl -----------------------------------------------
dnl Check for GCC pedantic support
dnl -----------------------------------------------
AC_ARG_ENABLE(gcc_warn_pedantic,
              AS_HELP_STRING([--disable-gcc-warn-pedantic], [Disable GCC pedantic warnings demanded by strict ISO C and ISO C++, this option uses the standard used in --with-gcc-c-standard]))
    if test "$enable_gcc_warn_pedantic" != "no"; then

        if test "$GCC_C_STD_SET" = "yes"; then
            save_CFLAGS="${CFLAGS}"

            AC_MSG_CHECKING(gcc pedantic C warnings)
            CFLAGS="${CFLAGS} ${GCC_CHARACTERISTICS_CFLAGS} -pedantic"
            AC_TRY_COMPILE(,,[gcc_have_warn_c_pedantic=yes],[gcc_have_warn_c_pedantic=no])
            if test "$gcc_have_warn_c_pedantic" = "yes"; then
                AC_MSG_RESULT(yes)
                GCC_CHARACTERISTICS_CFLAGS="${GCC_CHARACTERISTICS_CFLAGS} -pedantic"
            else
                AC_MSG_RESULT(no)
                echo
                echo "   You specified -pedantic should be set but it was not added"
                echo "   as it appears to be unsupported by your compiler."
                echo
            fi
            CFLAGS="${save_CFLAGS}"
        else
            echo
            echo "   You specified -pedantic should set but it was not added as"
            echo "   -std flag was not set"
            echo
        fi
 
        if test "$GCC_CPP_STD_SET" = "yes"; then
            save_CXXFLAGS="${CFLAGS}"

            AC_MSG_CHECKING(gcc pedantic C++ warnings)
            AC_LANG_SAVE
            AC_LANG_CPLUSPLUS
            CXXFLAGS="${CXXFLAGS} ${GCC_CHARACTERISTICS_CXXFLAGS} -pedantic"
            AC_TRY_COMPILE(,,[gcc_have_warn_cpp_pedantic=yes],[gcc_have_warn_cpp_pedantic=no])
            if test "$gcc_have_warn_cpp_pedantic" = "yes"; then
                AC_MSG_RESULT(yes)
                GCC_CHARACTERISTICS_CXXFLAGS="${GCC_CHARACTERISTICS_CXXFLAGS} -pedantic"
            else
                AC_MSG_RESULT(no)
                echo
                echo "   You specified -pedantic should set but it was not added"
                echo "   as it appears to be unsupported by your compiler."
                echo
            fi
            AC_LANG_RESTORE
            CXXFLAGS="${save_CXXFLAGS}"
        else
            AC_MSG_RESULT(no)
            echo
            echo "   You specified -pedantic should be set but it was not added"
            echo "   as it appears to be unsupported by your compiler."
            echo
        fi

    else
        AC_MSG_RESULT(no)
    fi
echo
echo "GCC CHARACTERISTICS"
echo "CFLAGS=${GCC_CHARACTERISTICS_CFLAGS} CXXFLAGS=${GCC_CHARACTERISTICS_CXXFLAGS} CPPFLAGS=${GCC_CHARACTERISTICS_CPPFLAGS} LDFLAGS=${GCC_CHARACTERISTICS_LDFLAGS}"
echo
dnl echo "REAL CFLAGS=${CFLAGS} REAL CXXFLAGS=${CXXFLAGS} REAL CPPFLAGS=${CPPFLAGS} REAL LDFLAGS=${LDFLAGS}"
AC_SUBST(GCC_CHARACTERISTICS_CFLAGS)
AC_SUBST(GCC_CHARACTERISTICS_CPPFLAGS)
AC_SUBST(GCC_CHARACTERISTICS_CXXFLAGS)
AC_SUBST(GCC_CHARACTERISTICS_LDFLAGS)
])
