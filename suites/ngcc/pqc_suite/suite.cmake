# Generic suite for one NGCC round-1 candidate, built from its official
# submission package under downloads/ngcc/ (never committed):
#
#   tools/ngcc/ngcc gen <candidate> <parameter set>      (writes build/ngcc_gen/...)
#   tools/pqc run ngcc -D NGCC_CAND=<candidate> -D NGCC_PARAM=<parameter set> --build-name ngcc_<...>
#
# The candidate's reference code is compiled as its own static library, with
# its own include paths, so that its file and symbol names cannot collide
# with this project's. Shared by the device build and the host build.
set(SUITE_DIR ${CMAKE_CURRENT_LIST_DIR})
get_filename_component(NGCC_ROOT ${SUITE_DIR}/../../.. ABSOLUTE)

set(NGCC_CAND "" CACHE STRING "NGCC candidate id, e.g. kem_CheetahKEM")
set(NGCC_PARAM "" CACHE STRING "Parameter set of the candidate, e.g. Cheetah128")
set(NGCC_STACK "0" CACHE STRING "Task stack for each job in bytes (0 = choose from the free heap)")
set(NGCC_STACK_PCT "60" CACHE STRING "With NGCC_STACK=0: share of the largest free block given to a job's stack")
set(NGCC_ITERS "3" CACHE STRING "Timed iterations per job")
set(NGCC_GEN ${NGCC_ROOT}/build/ngcc_gen/${NGCC_CAND}__${NGCC_PARAM})

set(SUITE_SRCS ${SUITE_DIR}/suite.c)
set(SUITE_INCS ${SUITE_DIR})
set(SUITE_COPTS)
set(SUITE_DEVICE_SRCS)

if(EXISTS ${NGCC_GEN}/cand.cmake)
    # Sets NGCC_KIND, NGCC_ALG, NGCC_SRCS, NGCC_DRNG_SRC, NGCC_INCS, NGCC_DEFS,
    # NGCC_COPTS, NGCC_RB_LEN, NGCC_SHIM_SRCS.
    include(${NGCC_GEN}/cand.cmake)
elseif(NOT CMAKE_SCRIPT_MODE_FILE)
    message(FATAL_ERROR "No generated description for '${NGCC_CAND}' / '${NGCC_PARAM}'. "
                        "Run: tools/ngcc/ngcc gen ${NGCC_CAND} ${NGCC_PARAM}")
endif()

set(SUITE_DEFS NGCC_KIND_${NGCC_KIND}=1 NGCC_ALG="${NGCC_ALG}" NGCC_STACK=${NGCC_STACK}
               NGCC_STACK_PCT=${NGCC_STACK_PCT} NGCC_ITERS=${NGCC_ITERS} NGCC_RB_LEN=${NGCC_RB_LEN}
               ${NGCC_SUITE_DEFS})

# Files that do not compile for RV32 are left out of the device build; if the
# candidate needs them, the link fails and the tool reports why.
if(NOT PQC_HOST AND NGCC_RV_EXCLUDE)
    list(REMOVE_ITEM NGCC_SRCS ${NGCC_RV_EXCLUDE})
endif()

# Hash shims: this project's replacements for a candidate's own hash code,
# built on components/prims so that the backend switch applies. They are part
# of the suite (they need this project's headers) but use the candidate's
# symbol names.
foreach(src ${NGCC_SHIM_SRCS})
    list(APPEND SUITE_SRCS ${src})
    set_property(SOURCE ${src} APPEND PROPERTY COMPILE_OPTIONS -include ${NGCC_GEN}/ngcc_rename.h)
    set_property(SOURCE ${src} APPEND PROPERTY INCLUDE_DIRECTORIES ${NGCC_INCS})
endforeach()

# The candidate library. Called once the toolchain is known (not in ESP-IDF's
# early script pass).
function(ngcc_add_candidate_library)
    add_library(ngcc_cand STATIC ${NGCC_SRCS} ${NGCC_DRNG_SRC})
    target_include_directories(ngcc_cand PRIVATE ${NGCC_INCS})
    # Small stand-ins for headers that one side lacks (malloc.h, endian.h, ...).
    if(PQC_HOST)
        target_include_directories(ngcc_cand PRIVATE ${NGCC_ROOT}/tools/ngcc/compat/host)
    else()
        target_include_directories(ngcc_cand PRIVATE ${NGCC_ROOT}/tools/ngcc/compat/device)
    endif()
    target_compile_definitions(ngcc_cand PRIVATE ${NGCC_DEFS})
    target_compile_options(ngcc_cand PRIVATE -w -fcommon ${NGCC_COPTS}
        -Wno-error=implicit-function-declaration -Wno-error=implicit-int
        -Wno-error=incompatible-pointer-types -Wno-error=int-conversion
        -Wno-error=return-type)
    # Every candidate source sees the rename header; the ICCS generator file
    # gets its own, so that its entry points can be wrapped.
    foreach(src ${NGCC_SRCS})
        set_property(SOURCE ${src} APPEND PROPERTY COMPILE_OPTIONS -include ${NGCC_GEN}/ngcc_rename.h)
    endforeach()
    foreach(src ${NGCC_DRNG_SRC})
        set_property(SOURCE ${src} APPEND PROPERTY COMPILE_OPTIONS -include ${NGCC_GEN}/ngcc_rename_drng.h)
    endforeach()
    # A file with its own randombytes() built on the ICCS generator keeps it
    # under a private name.
    foreach(src ${NGCC_OWN_RB_SRCS})
        set_property(SOURCE ${src} APPEND PROPERTY COMPILE_OPTIONS -include ${NGCC_GEN}/ngcc_rename_ownrb.h)
    endforeach()
endfunction()

if(PQC_HOST)
    ngcc_add_candidate_library()
    target_sources(ngcc_cand PRIVATE ${NGCC_ROOT}/tools/ngcc/compat/host/compat.c)
    target_compile_options(ngcc_cand PRIVATE -O2)
    link_libraries(ngcc_cand)
    if(NGCC_HOST_LINK_OPTS)
        add_link_options(${NGCC_HOST_LINK_OPTS})
    endif()
endif()
