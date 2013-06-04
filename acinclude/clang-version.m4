AC_DEFUN([AX_CLANG_VERSION], [
  CLANG_VERSION=""
  AS_IF([test "x$cc_is_clang" = "xyes"],[
    AC_CACHE_CHECK([clang version],[ax_cv_clang_version],[
      ax_cv_clang_version="`$CC --version | head -1 | egrep -o '3\.\d'`"
      AS_IF([test "x$ax_cv_clang_version" = "x"],[
        ax_cv_clang_version=""
      ])
    ])
    CLANG_VERSION=$ax_cv_clang_version
  ])
  AC_SUBST([CLANG_VERSION])
])
