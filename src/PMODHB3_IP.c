
/***************************** Include Files *******************************/
#include "PMODHB3_IP.h"
#include <stdbool.h>
#include "xil_types.h"
#include "xparameters.h"
#include "xil_io.h"

/***************************** Global variables ****************************/
static u32 baseAddress = 0L;
static bool isInitialized = false;

/************************** Function Definitions ***************************/

XStatus PMODHB3_Initialize(u32 baseaddr_p)
{
    //XStatus sts;
    if (baseaddr_p == NULL) {
        isInitialized = false;
        return XST_FAILURE;  
    }
    
    if (isInitialized) {
        return XST_SUCCESS;
    }
    else {
        baseAddress = baseaddr_p;
        //sts = PMODHB3_AXI_IP_Reg_SelfTest(baseAddress);
        //if (sts != XST_SUCCESS)
        //    return XST_FAILURE;
        
        isInitialized = true;
    }
    return XST_SUCCESS; 
}

u32 PMODHB3_GetRpm(u32 baseAddress, u32 offset)
{
    if(isInitialized)
    {
        return PMODHB3_IP_mReadReg(baseAddress, offset);
    }
    else
    {
        return 0xFFFFFFFF;
    }
}

XStatus PMODHB3_SetConfig(u32 baseAddress, u32 offset, u32 data)
{

    if(isInitialized)
    {
        PMODHB3_IP_mWriteReg(baseAddress, offset, data);
        return XST_SUCCESS;
    }
    
    return XST_FAILURE;
}
