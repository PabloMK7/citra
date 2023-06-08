# Gets a UTC timstamp and sets the provided variable to it
function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

list(APPEND CMAKE_MODULE_PATH "${SRC_DIR}/externals/cmake-modules")

# Find the package here with the known path so that the GetGit commands can find it as well
find_package(Git QUIET PATHS "${GIT_EXECUTABLE}")

# generate git/build information
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REF_SPEC GIT_REV)
string(SUBSTRING "${GIT_REV}" 0 7 GIT_SHORT_REV)
git_describe(GIT_DESC --always --long --dirty)
git_branch_name(GIT_BRANCH)
get_timestamp(BUILD_DATE)

# Generate cpp with Git revision from template
# Also if this is a CI build, add the build name (ie: Nightly, Canary) to the scm_rev file as well
set(REPO_NAME "")
set(BUILD_VERSION "0")
set(BUILD_FULLNAME "${GIT_SHORT_REV}")
if (DEFINED ENV{CI})
    if (DEFINED ENV{GITHUB_ACTIONS})
        set(BUILD_REPOSITORY $ENV{GITHUB_REPOSITORY})
        set(BUILD_TAG $ENV{GITHUB_REF_NAME})
    endif()

    # regex capture the string nightly or canary into CMAKE_MATCH_1
    string(REGEX MATCH "citra-emu/citra-?(.*)" OUTVAR ${BUILD_REPOSITORY})
    if ("${CMAKE_MATCH_COUNT}" GREATER 0)
        # capitalize the first letter of each word in the repo name.
        string(REPLACE "-" ";" REPO_NAME_LIST ${CMAKE_MATCH_1})
        foreach(WORD ${REPO_NAME_LIST})
            string(SUBSTRING ${WORD} 0 1 FIRST_LETTER)
            string(SUBSTRING ${WORD} 1 -1 REMAINDER)
            string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
            set(REPO_NAME "${REPO_NAME}${FIRST_LETTER}${REMAINDER}")
        endforeach()
        string(REGEX MATCH "${CMAKE_MATCH_1}-([0-9]+)" OUTVAR ${BUILD_TAG})
        if (${CMAKE_MATCH_COUNT} GREATER 0)
            set(BUILD_VERSION ${CMAKE_MATCH_1})
        endif()
        if (BUILD_VERSION)
            # This leaves a trailing space on the last word, but we actually want that
            # because of how it's styled in the title bar.
            set(BUILD_FULLNAME "${REPO_NAME} ${BUILD_VERSION} ")
        else()
            set(BUILD_FULLNAME "")
        endif()
    endif()
endif()
