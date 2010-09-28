include( TestBigEndian)

TEST_BIG_ENDIAN(IS_BIGENDIAN)

if (${IS_BIGENDIAN})

	set( GLIB_ENDIANESS G_BIG_ENDIAN)

else (${IS_BIGENDIAN})

	set( GLIB_ENDIANESS G_LITTLE_ENDIAN)
endif (${IS_BIGENDIAN})
