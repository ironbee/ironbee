dnl Check for OSSP_UUID utility
dnl CHECK_OSSP_UUID(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  OSSP_UUID
dnl  OSSP_UUID_CFLAGS
dnl  OSSP_UUID_LDFLAGS
dnl  OSSP_UUID_LIBS
dnl  OSSP_UUID_MODULES

OSSP_UUID=""
OSSP_UUID_CFLAGS=""
OSSP_UUID_CPPFLAGS=""
OSSP_UUID_LDFLAGS=""
OSSP_UUID_LIBS=""

AC_DEFUN([CHECK_OSSP_UUID],
[dnl

AC_ARG_WITH(uuid_config_prg,
            [  --with-uuid-config-prg=NAME Name of the uuid-config program],
            [with_uuid_config_prg="$withval"],
            [with_uuid_config_prg="uuid-config"])

AC_ARG_WITH(uuid_config,
            [  --with-uuid-config=PATH  Path to uuid-config directory],
            [with_uuid_config="$withval"],
            [with_uuid_config="no"])

if test "$with_uuid_config" = "no"; then
   with_uuid_config=$PATH
   uuid_config_specified="no"
else
   uuid_config_specified="yes"
fi

AC_PATH_PROG(UUID_CONFIG, $with_uuid_config_prg, no, $with_uuid_config)
if test "$UUID_CONFIG" = "no"; then
    if test "$uuid_config_specified" = "yes"; then
        AC_MSG_ERROR([The path specified for uuid-config doesn't seem to contain the program uuid-config.  Please re-check the supplied path."])
    else
        AC_MSG_ERROR([Please install ossp-uuid.  If you have ossp-uuid installed in a non-standard location, please specify a path to uuid-config using --with-uuid-config=PATH."])
    fi
fi

OSSP_UUID_CFLAGS=`${UUID_CONFIG} --cflags`
OSSP_UUID_LDFLAGS=`${UUID_CONFIG} --ldflags`
OSSP_UUID_LIBS=`${UUID_CONFIG} --libs`

AC_SUBST(OSSP_UUID_CFLAGS)
AC_SUBST(OSSP_UUID_LDFLAGS)
AC_SUBST(OSSP_UUID_LIBS)
]
)
