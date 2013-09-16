# ==================== Ruby ====================
AC_DEFUN([CHECK_RUBY],
[
  AC_PATH_PROG([RUBY], ruby)

  if test "x$RUBY" == "x" ; then
    AC_MSG_ERROR([ruby is required])
  fi

  minRubyVersion=[$1]
  AC_MSG_CHECKING([for ruby minimum version $minRubyVersion])

  ## warning about line below: use $ 2 instead of $2 otherwise m4 trys to
  ## interpret, luckily awk doesn't care
  rubyVersion=`$RUBY --version | awk '{print $ 2}'`

  AX_COMPARE_VERSION([$rubyVersion],[ge],[$minRubyVersion],
       [AC_MSG_RESULT($rubyVersion is ok)],
       [AC_MSG_ERROR([ruby version must be >= $minRubyVersion - found $rubyVersion])])

   RUBY_TOPDIR=`$RUBY -rrbconfig -e  "print \"#{RbConfig::CONFIG['topdir']}\n\""`
   RUBY_LIBRUBYARG=`$RUBY -rrbconfig -e "print \"#{RbConfig::CONFIG['LIBRUBYARG']}\n\""`
   RUBY_LIBDIR=`$RUBY -rrbconfig -e "print \"#{RbConfig::CONFIG['libdir']}\n\""`
   AC_SUBST(RUBY_TOPDIR)
   AC_SUBST(RUBY_LIBRUBYARG)
   AC_SUBST(RUBY_LIBDIR)
])

# ==================== Ruby Gem ====================
# Like Perl's CPAN
AC_DEFUN([CHECK_GEM],
[
  AC_PATH_PROG([GEM], gem)

  minGemVersion=[$1]
  if test "x$GEM" == "x" ; then
    AC_MSG_ERROR([ruby gem command is required])
  fi

  AC_MSG_CHECKING([for gem minimum version $minGemVersion])

  gemVersion=`$GEM --version`

  AX_COMPARE_VERSION([$gemVersion],[ge],[$minGemVersion],
       [AC_MSG_RESULT($gemVersion is ok)],
       [AC_MSG_ERROR([gem version must be >= $minGemVersion - found $gemVersion])])
])

##
##  pass module path (e.g. wsdl/soap/wsl2ruby)
##
AC_DEFUN([CHECK_RUBY_GEM],
[
  rubyGem=[$1]
	minRubyGemVersion=[$2]

	AC_CACHE_CHECK([for ruby gem $rubyGem],[$rubyGem],[
	  acl_cv_gem_[$3]=`$GEM list --local [$1] | egrep "^[$1]" | awk '{print $ 2}'| sed -e 's/@<:@(),@:>@//g'`
	])
	rubyGemVersion=$acl_cv_gem_[$3]
	AC_MSG_CHECKING([for [$1] minimum version $minRubyGemVersion])
	if test x"$acl_cv_gem_[$3]" == x; then
    AC_MSG_RESULT([missing])
    AC_MSG_ERROR([type 'gem install $rubyGem' to install])
  else
	  AX_COMPARE_VERSION([$rubyGemVersion],[ge],$[minRubyGemVersion],
		  [AC_MSG_RESULT($rubyGemVersion is ok)],
		  [AC_MSG_ERROR([$rubyGem version must be >= $minRubyGemVersion - found $rubyGemVersion])])
  fi
])
