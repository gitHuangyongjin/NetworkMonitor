project(GATEWAY_VSOL_APP_VSDIAG)
include(${CONFIG_CMAKE_DIR}/toolchains/${CONFIG_TOOLCHAIN_LINUX_APP}.cmake)
# 组件名称
set(USERAPP_NAME vsdiag)

# 组件类型
set(USERAPP_TARGET_TYPE bin)

# 依赖的源文件
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} VSOL_APP_VSDIAG)
set(USERAPP_PRIVATE_SRC
    ${VSOL_APP_VSDIAG}
)

# 依赖的头文件
set(USERAPP_PRIVATE_INC
    ${HGW_SERVICE_DIR}/dms/board/include
    ${HGW_SERVICE_DIR}/dms/sys_env/libenv/include
    ${HGW_SERVICE_DIR}/network/emu/u_space/include
    ${HGW_FWK_DIR}/include
    ${HGW_FWK_DIR}/ipc/include
    ${HGW_FWK_DIR}/util/include
    ${HGW_FWK_DIR}/notifier/include
    ${HGW_SERVICE_DIR}/dms/sysinfo/include
    ${HGW_SERVICE_DIR}/dms/cfm/cfm_lib
    ${HGW_SERVICE_DIR}/dms/led/include
    ${HGW_SERVICE_DIR}/voice/slic/u_space/include
    ${HGW_SERVICE_DIR}/voice/voip/uvoipapi/include
    ${CONFIG_GATEWAY_DIR}/cml/odl/include
    ${CONFIG_GATEWAY_DIR}/cml/odlapi/include
    ${CONFIG_GATEWAY_DIR}/basic/include
    ${CONFIG_OPENSRC_INCLUDE_DIR}/extservice
    ${CONFIG_OPENSRC_INCLUDE_DIR}/ezxml
    ${CONFIG_BASIC_DIR}/include
    ${CONFIG_BASIC_DIR}/linux
    ${CONFIG_OPENSRC_INCLUDE_DIR}/../rootfs/include
    ${HGW_SERVICE_DIR}/network/easymesh/controller_api
    ${HGW_SERVICE_DIR}/network/easymesh/libs/include
    ${HGW_SERVICE_DIR}/network/easymesh/agent_api

)

# 依赖的库
set(USERAPP_PRIVATE_LIB
    extservice
    securec
    xml2
    hi_sysenv
    hi_util
    hi_ipc
    hi_ioreactor
    hi_basic
    hi_odlapi
    hi_cfm
    hi_emagentapi
    hi_emctrlapi
    # dmallocth
    gcc_s

)

# 自定义宏
set(USERAPP_PRIVATE_DEFINE)

# 自定义编译选项
set(USERAPP_PRIVATE_COMPILE
    ${HGW_COMMON_CFLAGS}
)

build_app_feature()
