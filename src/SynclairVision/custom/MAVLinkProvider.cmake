include_guard(GLOBAL)

set(_synclair_definitions "${CMAKE_SOURCE_DIR}/message-definitions")
set(_synclair_mavlink "${_synclair_definitions}/mavlink")
set(_synclair_xml_source "${_synclair_mavlink}/message_definitions/v1.0")
set(_synclair_binary "${CMAKE_BINARY_DIR}/SynclairVision/message-definitions")

foreach(_required IN ITEMS
    "${_synclair_xml_source}/all.xml"
    "${_synclair_definitions}/sv_mavlink_dialect.xml"
    "${_synclair_definitions}/copilot_mavlink_dialect.xml"
)
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR
            "Synclair: Required MAVLink definitions are missing: ${_required}. "
            "Run 'git submodule update --init --recursive' or 'just submodules', then configure again."
        )
    endif()
endforeach()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(QGC_MAVLINK_GENERATED_DIR "${_synclair_binary}/generated/include/mavlink/v${QGC_MAVLINK_VERSION}")
set(QGC_MAVLINK_XML_DIR "${_synclair_binary}/mavlink/message_definitions/v1.0")
set(QGC_MAVLINK_COMPONENT_METADATA_DIR "${_synclair_mavlink}/component_metadata")
set(_synclair_stage_script "${CMAKE_CURRENT_LIST_DIR}/StageMAVLinkDefinitions.cmake")
set(_synclair_generated_header "${QGC_MAVLINK_GENERATED_DIR}/all/mavlink.h")

file(GLOB _synclair_xml_files CONFIGURE_DEPENDS "${_synclair_xml_source}/*.xml")

add_custom_command(
    OUTPUT "${_synclair_generated_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${QGC_MAVLINK_GENERATED_DIR}"
    COMMAND "${CMAKE_COMMAND}"
        "-DDEFINITIONS_SOURCE=${_synclair_definitions}"
        "-DXML_OUTPUT=${QGC_MAVLINK_XML_DIR}"
        -P "${_synclair_stage_script}"
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${_synclair_mavlink}"
        "${Python3_EXECUTABLE}" -m pymavlink.tools.mavgen
        --lang=C "--wire-protocol=${QGC_MAVLINK_VERSION}" --no-validate
        "--output=${QGC_MAVLINK_GENERATED_DIR}" "${QGC_MAVLINK_XML_DIR}/all.xml"
    DEPENDS
        ${_synclair_xml_files}
        "${_synclair_definitions}/sv_mavlink_dialect.xml"
        "${_synclair_definitions}/copilot_mavlink_dialect.xml"
        "${_synclair_stage_script}"
    COMMENT "Generating standard, DigiView and Copilot MAVLink headers"
    VERBATIM
)

add_custom_target(synclair_mavlink_generate_headers DEPENDS "${_synclair_generated_header}")
add_library(mavlink INTERFACE)
add_dependencies(mavlink synclair_mavlink_generate_headers)
target_include_directories(mavlink INTERFACE
    "${QGC_MAVLINK_GENERATED_DIR}"
    "${QGC_MAVLINK_GENERATED_DIR}/${QGC_MAVLINK_DIALECT}"
)
