include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/EasierCppPch.cmake")
# The cache value survives until the deferred call; targets in subdirectories
# exist by then. Recipes are explicit, trusted user configuration snapshots.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL include "${EASIERCPP_PCH_CONFIG}")
