function(check_type_exists typename)
    check_type_size(${typename} ${typename})

    if (NOT ${HAVE_${typename}})
        message(FATAL_ERROR "Unable to find type '${typename}'")
    endif()
endfunction()