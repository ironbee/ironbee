AC_DEFUN([AX_GCC_VERSION], [
  GCC_VERSION=""
  AS_IF([test "x$GCC" = "xyes"],[
    AC_CACHE_CHECK([gcc version],[ax_cv_gcc_version],[
      ax_cv_gcc_version="`$CC -dumpversion`"
      AS_IF([test "x$ax_cv_gcc_version" = "x"],[
        ax_cv_gcc_version=""
      ])
    ])
    GCC_VERSION=$ax_cv_gcc_version
  ])
  AC_SUBST([GCC_VERSION])
])
