find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(nassp_resource_inputs
  "${NASSP_SOURCE_ROOT}/src_csm/Resources.rc"
  "${NASSP_SOURCE_ROOT}/src_csm/resource.h"
  "${NASSP_SOURCE_ROOT}/src_lm/LEMResources.rc"
  "${NASSP_SOURCE_ROOT}/src_lm/lmresource.h"
  "${NASSP_SOURCE_ROOT}/src_mfd/MFDResources.rc"
  "${NASSP_SOURCE_ROOT}/src_mfd/MFDResource.h")
file(GLOB_RECURSE nassp_bitmap_inputs CONFIGURE_DEPENDS
  "${NASSP_SOURCE_ROOT}/Bitmaps/*.[bB][mM][pP]")
set(nassp_resource_header "${PROJECT_BINARY_DIR}/generated/NasspBitmapResources.h")
set(nassp_resource_manifest "${PROJECT_BINARY_DIR}/generated/nassp-bitmaps.json")
# Expanded resources live outside the source tree; runtime loading uses stb_image.
set(nassp_bitmap_prefix "Resources/NASSP")
set(nassp_bitmap_output "${PROJECT_BINARY_DIR}/runtime/${nassp_bitmap_prefix}")
set(nassp_bitmap_outputs)
foreach(input IN LISTS nassp_bitmap_inputs)
  file(RELATIVE_PATH relative "${NASSP_SOURCE_ROOT}/Bitmaps" "${input}")
  list(APPEND nassp_bitmap_outputs "${nassp_bitmap_output}/${relative}")
endforeach()
add_custom_command(
  OUTPUT "${nassp_resource_header}" "${nassp_resource_manifest}"
  BYPRODUCTS ${nassp_bitmap_outputs}
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/nassp_bitmap_resources.py"
    --source-root "${NASSP_SOURCE_ROOT}"
    --asset-prefix "${nassp_bitmap_prefix}" --output-bitmaps "${nassp_bitmap_output}"
    --header "${nassp_resource_header}" --manifest "${nassp_resource_manifest}"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/nassp_bitmap_resources.py" "${PROJECT_SOURCE_DIR}/tools/bmp_normalize.py"
    ${nassp_resource_inputs} ${nassp_bitmap_inputs}
  VERBATIM)
add_custom_target(nassp_bitmap_resources DEPENDS "${nassp_resource_header}" "${nassp_resource_manifest}")
add_library(NASSPBitmapResources INTERFACE)
target_include_directories(NASSPBitmapResources INTERFACE "${PROJECT_BINARY_DIR}/generated")
add_dependencies(NASSPBitmapResources nassp_bitmap_resources)

add_executable(nassp_bitmap_resources_test "${PROJECT_SOURCE_DIR}/Tests/NasspBitmapResources.cpp")
target_link_libraries(nassp_bitmap_resources_test PRIVATE NASSPBitmapResources)
add_test(NAME nassp_bitmap_resources COMMAND nassp_bitmap_resources_test "${PROJECT_BINARY_DIR}/runtime")
add_test(NAME nassp_bmp_normalize COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tests/BmpNormalize.py")
