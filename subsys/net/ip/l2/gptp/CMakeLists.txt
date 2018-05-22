zephyr_library()
zephyr_library_include_directories(. ../..)
zephyr_library_compile_definitions_ifdef(
  CONFIG_NEWLIB_LIBC __LINUX_ERRNO_EXTENSIONS__
  )

zephyr_library_sources(
  gptp.c
  gptp_iface.c
  gptp_md.c
  gptp_messages.c
  gptp_mi.c
  )

set_source_files_properties( gptp.c           PROPERTIES COMPILE_FLAGS -finstrument-functions)
set_source_files_properties( gptp_iface.c     PROPERTIES COMPILE_FLAGS -finstrument-functions)
set_source_files_properties( gptp_md.c        PROPERTIES COMPILE_FLAGS -finstrument-functions)
set_source_files_properties( gptp_messages.c  PROPERTIES COMPILE_FLAGS -finstrument-functions)
set_source_files_properties( gptp_mi.c        PROPERTIES COMPILE_FLAGS -finstrument-functions)
