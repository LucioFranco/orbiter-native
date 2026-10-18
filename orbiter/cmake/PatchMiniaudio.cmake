# miniaudio 0.11.25 frees a failed resource node before inspecting its ownership
# flag while cleaning up notifications. Keep the pinned upstream checkout intact
# and compile a private header with cleanup reordered before the free.
file(READ "${miniaudio_SOURCE_DIR}/miniaudio.h" miniaudio_header)
set(miniaudio_failed_node_cleanup [=[    /* If we failed to initialize the data buffer we need to free it. */
    if (result != MA_SUCCESS) {
        if (nodeAlreadyExists == MA_FALSE) {
            ma_resource_manager_data_buffer_node_remove(pResourceManager, pDataBufferNode);
            ma_free(pDataBufferNode, &pResourceManager->config.allocationCallbacks);
        }
    }

]=])
set(miniaudio_output_node [=[    if (ppDataBufferNode != NULL) {
        *ppDataBufferNode = pDataBufferNode;
    }

    return result;
}

static ma_result ma_resource_manager_data_buffer_node_unacquire]=])
string(FIND "${miniaudio_header}" "${miniaudio_failed_node_cleanup}" cleanup_offset)
string(FIND "${miniaudio_header}" "${miniaudio_output_node}" output_offset)
if(cleanup_offset LESS 0 OR output_offset LESS 0)
  message(FATAL_ERROR "Pinned miniaudio cleanup patch no longer matches; inspect the dependency before updating it")
endif()
string(REPLACE "${miniaudio_failed_node_cleanup}" "" miniaudio_header "${miniaudio_header}")
string(REPLACE "            ma_free(pDataBufferNode, &pResourceManager->config.allocationCallbacks);"
  "            ma_free(pDataBufferNode, &pResourceManager->config.allocationCallbacks);\n            pDataBufferNode = NULL;"
  miniaudio_failed_node_cleanup "${miniaudio_failed_node_cleanup}")
string(REPLACE "${miniaudio_output_node}" "${miniaudio_failed_node_cleanup}${miniaudio_output_node}"
  miniaudio_header "${miniaudio_header}")
set(miniaudio_private_include "${CMAKE_CURRENT_BINARY_DIR}/native-miniaudio")
file(MAKE_DIRECTORY "${miniaudio_private_include}")
set(miniaudio_existing_header "")
if(EXISTS "${miniaudio_private_include}/miniaudio.h")
  file(READ "${miniaudio_private_include}/miniaudio.h" miniaudio_existing_header)
endif()
if(NOT miniaudio_existing_header STREQUAL miniaudio_header)
  file(WRITE "${miniaudio_private_include}/miniaudio.h" "${miniaudio_header}")
endif()
unset(miniaudio_header)
unset(miniaudio_existing_header)
