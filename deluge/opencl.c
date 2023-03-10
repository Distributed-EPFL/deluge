#include "deluge/opencl.h"


#define __ERR_STRING(x) case x: return #x;

const char *opencl_errstr(cl_int err)
{
        switch (err) {
        __ERR_STRING(CL_SUCCESS                        )
        __ERR_STRING(CL_DEVICE_NOT_FOUND               )
        __ERR_STRING(CL_DEVICE_NOT_AVAILABLE           )
        __ERR_STRING(CL_COMPILER_NOT_AVAILABLE         ) 
        __ERR_STRING(CL_MEM_OBJECT_ALLOCATION_FAILURE  )
        __ERR_STRING(CL_OUT_OF_RESOURCES               )
        __ERR_STRING(CL_OUT_OF_HOST_MEMORY             )
        __ERR_STRING(CL_PROFILING_INFO_NOT_AVAILABLE   )
        __ERR_STRING(CL_MEM_COPY_OVERLAP               )
        __ERR_STRING(CL_IMAGE_FORMAT_MISMATCH          )
        __ERR_STRING(CL_IMAGE_FORMAT_NOT_SUPPORTED     )
        __ERR_STRING(CL_BUILD_PROGRAM_FAILURE          )
        __ERR_STRING(CL_MAP_FAILURE                    )
        __ERR_STRING(CL_MISALIGNED_SUB_BUFFER_OFFSET   )
        __ERR_STRING(CL_COMPILE_PROGRAM_FAILURE        )
        __ERR_STRING(CL_LINKER_NOT_AVAILABLE           )
        __ERR_STRING(CL_LINK_PROGRAM_FAILURE           )
        __ERR_STRING(CL_DEVICE_PARTITION_FAILED        )
        __ERR_STRING(CL_KERNEL_ARG_INFO_NOT_AVAILABLE  )
        __ERR_STRING(CL_INVALID_VALUE                  )
        __ERR_STRING(CL_INVALID_DEVICE_TYPE            )
        __ERR_STRING(CL_INVALID_PLATFORM               )
        __ERR_STRING(CL_INVALID_DEVICE                 )
        __ERR_STRING(CL_INVALID_CONTEXT                )
        __ERR_STRING(CL_INVALID_QUEUE_PROPERTIES       )
        __ERR_STRING(CL_INVALID_COMMAND_QUEUE          )
        __ERR_STRING(CL_INVALID_HOST_PTR               )
        __ERR_STRING(CL_INVALID_MEM_OBJECT             )
        __ERR_STRING(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR)
        __ERR_STRING(CL_INVALID_IMAGE_SIZE             )
        __ERR_STRING(CL_INVALID_SAMPLER                )
        __ERR_STRING(CL_INVALID_BINARY                 )
        __ERR_STRING(CL_INVALID_BUILD_OPTIONS          )
        __ERR_STRING(CL_INVALID_PROGRAM                )
        __ERR_STRING(CL_INVALID_PROGRAM_EXECUTABLE     )
        __ERR_STRING(CL_INVALID_KERNEL_NAME            )
        __ERR_STRING(CL_INVALID_KERNEL_DEFINITION      )
        __ERR_STRING(CL_INVALID_KERNEL                 )
        __ERR_STRING(CL_INVALID_ARG_INDEX              )
        __ERR_STRING(CL_INVALID_ARG_VALUE              )
        __ERR_STRING(CL_INVALID_ARG_SIZE               )
        __ERR_STRING(CL_INVALID_KERNEL_ARGS            )
        __ERR_STRING(CL_INVALID_WORK_DIMENSION         )
        __ERR_STRING(CL_INVALID_WORK_GROUP_SIZE        )
        __ERR_STRING(CL_INVALID_WORK_ITEM_SIZE         )
        __ERR_STRING(CL_INVALID_GLOBAL_OFFSET          )
        __ERR_STRING(CL_INVALID_EVENT_WAIT_LIST        )
        __ERR_STRING(CL_INVALID_EVENT                  )
        __ERR_STRING(CL_INVALID_OPERATION              )
        __ERR_STRING(CL_INVALID_GL_OBJECT              )
        __ERR_STRING(CL_INVALID_BUFFER_SIZE            )
        __ERR_STRING(CL_INVALID_MIP_LEVEL              )
        __ERR_STRING(CL_INVALID_GLOBAL_WORK_SIZE       )
        __ERR_STRING(CL_INVALID_PROPERTY               )
        __ERR_STRING(CL_INVALID_IMAGE_DESCRIPTOR       )
        __ERR_STRING(CL_INVALID_COMPILER_OPTIONS       )
        __ERR_STRING(CL_INVALID_LINKER_OPTIONS         )
        __ERR_STRING(CL_INVALID_DEVICE_PARTITION_COUNT )
        default: return "Unknown OpenCL error code";
        }
}

#undef __ERR_STRING
