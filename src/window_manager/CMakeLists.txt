add_executable(noko
  main.cc
  ../protobuf/windowmanager.pb.cc
)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)
FetchContent_Declare(
  nanomsg
  GIT_REPOSITORY https://github.com/nanomsg/nanomsg
  GIT_TAG        1.2.2
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(nanomsg)
FetchContent_GetProperties(nanomsg)
if(NOT nanomsg_POPULATED)
  FetchContent_Populate(nanomsg)
endif()

target_include_directories(noko PRIVATE ${nanomsg_SOURCE_DIR}/src)


target_compile_options(noko PRIVATE -std=c++20 -g)

find_package(X11 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)
target_link_libraries(noko ${X11_LIBRARIES} Xext GL GLU GLEW Xcomposite Xfixes nanomsg protobuf::libprotobuf absl::log_internal_check_op absl::log_internal_message)
