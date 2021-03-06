/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "usb_srv_client.h"
#include <sstream>
#include "datetime_ex.h"
#include "if_system_ability_manager.h"
#include "ipc_skeleton.h"
#include "iservice_registry.h"
#include "string_ex.h"
#include "system_ability_definition.h"
#include "usb_common.h"
#include "usb_device.h"
#include "usb_errors.h"

namespace OHOS {
namespace USB {
#define USB_MAX_REQUEST_DATA_SIZE 1024

UsbSrvClient::UsbSrvClient()
{
    Connect();
}
UsbSrvClient::~UsbSrvClient() {}

void UsbSrvClient::PrintBuffer(const char *title, const uint8_t *buffer, uint32_t length)
{
    std::ostringstream oss;
    if (title == NULL || buffer == nullptr || length == 0) {
        return;
    }
    oss.str("");
    oss << title << " << 二进制数据流[" << length << "字节] >> :";
    for (uint32_t i = 0; i < length; ++i) {
        oss << " " << std::hex << (int)buffer[i];
    }
    oss << "  -->  " << buffer << std::endl;
    USB_HILOGD(MODULE_USB_INNERKIT, "%{public}s", oss.str().c_str());
}
int32_t UsbSrvClient::Connect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (proxy_ != nullptr) {
        return UEC_OK;
    }
    sptr<ISystemAbilityManager> sm = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (sm == nullptr) {
        USB_HILOGE(MODULE_USB_INNERKIT, "%{public}s:fail to get Registry", __func__);
        return UEC_INTERFACE_GET_SYSTEM_ABILITY_MANAGER_FAILED;
    }
    sptr<IRemoteObject> remoteObject_ = sm->CheckSystemAbility(USB_MANAGER_USB_SERVICE_ID);
    if (remoteObject_ == nullptr) {
        USB_HILOGE(MODULE_USB_INNERKIT, "GetSystemAbility failed.");
        return UEC_INTERFACE_GET_USB_SERVICE_FAILED;
    }
    proxy_ = iface_cast<IUsbSrv>(remoteObject_);
    USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s :Connect UsbService ok.", __func__);
    return UEC_OK;
}

void UsbSrvClient::ResetProxy(const wptr<IRemoteObject> &remote)
{
    std::lock_guard<std::mutex> lock(mutex_);
    RETURN_IF(proxy_ == nullptr);
    auto serviceRemote = proxy_->AsObject();
    if ((serviceRemote != nullptr) && (serviceRemote == remote.promote())) {
        serviceRemote->RemoveDeathRecipient(deathRecipient_);
        proxy_ = nullptr;
    }
}

void UsbSrvClient::UsbSrvDeathRecipient::OnRemoteDied(const wptr<IRemoteObject> &remote)
{
    if (remote == nullptr) {
        USB_HILOGE(MODULE_USB_INNERKIT, "UsbSrvDeathRecipient::OnRemoteDied failed, remote is nullptr.");
        return;
    }
    UsbSrvClient::GetInstance().ResetProxy(remote);
    USB_HILOGI(MODULE_USB_INNERKIT, "UsbSrvDeathRecipient::Recv death notice.");
}

int32_t UsbSrvClient::OpenDevice(const UsbDevice &device, USBDevicePipe &pipe)
{
    USB_HILOGI(MODULE_USB_INNERKIT, " Calling OpenDevice Start!");
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    int32_t ret = proxy_->OpenDevice(device.GetBusNum(), device.GetDevAddr());
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s : failed width ret = %{public}d !", __func__, ret);
        return ret;
    }

    pipe.SetBusNum(device.GetBusNum());
    pipe.SetDevAddr(device.GetDevAddr());
    return UEC_OK;
}

int32_t UsbSrvClient::HasRight(std::string deviceName)
{
    USB_HILOGI(MODULE_USB_INNERKIT, " Calling HasRight Start!");
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    int32_t ret = proxy_->HasRight(deviceName);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, " Calling HasRight False!");
    }
    return ret;
}

int32_t UsbSrvClient::RequestRight(std::string deviceName)
{
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    int32_t ret = proxy_->RequestRight(deviceName);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, " Calling RequestRight False!");
    }
    return ret;
}

int32_t UsbSrvClient::RemoveRight(std::string deviceName)
{
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    int32_t ret = proxy_->RemoveRight(deviceName);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, " Calling RequestRight False!");
    }
    return ret;
}

int32_t UsbSrvClient::GetDevices(std::vector<UsbDevice> &deviceList)
{
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    int32_t ret = proxy_->GetDevices(deviceList);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s failed ret = %{public}d!", __func__, ret);
    }
    USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s list size = %{public}d!", __func__, deviceList.size());
    return ret;
}

int32_t UsbSrvClient::GetCurrentFunctions(int32_t &funcs)
{
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    int32_t ret = proxy_->GetCurrentFunctions(funcs);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s failed ret = %{public}d!", __func__, ret);
    }
    USB_HILOGI(MODULE_USB_INNERKIT, " Calling GetCurrentFunctions Success!");
    return ret;
}
int32_t UsbSrvClient::SetCurrentFunctions(int32_t funcs)
{
    RETURN_IF_WITH_RET(Connect() != UEC_OK, false);
    int32_t ret = proxy_->SetCurrentFunctions(funcs);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s failed ret = %{public}d!", __func__, ret);
        return ret;
    }
    USB_HILOGI(MODULE_USB_INNERKIT, " Calling SetCurrentFunctions Success!");
    return ret;
}

int32_t UsbSrvClient::UsbFunctionsFromString(std::string funcs)
{
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    int32_t result = proxy_->UsbFunctionsFromString(funcs);
    USB_HILOGI(MODULE_USB_INNERKIT, " Calling UsbFunctionsFromString Success!");
    return result;
}

std::string UsbSrvClient::UsbFunctionsToString(int32_t funcs)
{
    std::string result;
    RETURN_IF_WITH_RET(Connect() != UEC_OK, result);
    result = proxy_->UsbFunctionsToString(funcs);
    USB_HILOGI(MODULE_USB_INNERKIT, " Calling UsbFunctionsToString Success!");
    return result;
}

int32_t UsbSrvClient::GetPorts(std::vector<UsbPort> &usbports)
{
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    USB_HILOGI(MODULE_USB_INNERKIT, " Calling GetPorts");
    int32_t ret = proxy_->GetPorts(usbports);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s failed ret = %{public}d!", __func__, ret);
    }
    return ret;
}

int32_t UsbSrvClient::GetSupportedModes(int32_t portId, int32_t &result)
{
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    USB_HILOGI(MODULE_USB_INNERKIT, " Calling GetSupportedModes");
    int32_t ret = proxy_->GetSupportedModes(portId, result);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s failed ret = %{public}d!", __func__, ret);
    }
    return ret;
}

int32_t UsbSrvClient::SetPortRole(int32_t portId, int32_t powerRole, int32_t dataRole)
{
    RETURN_IF_WITH_RET(Connect() != UEC_OK, UEC_INTERFACE_NO_INIT);
    USB_HILOGI(MODULE_USB_INNERKIT, " Calling SetPortRole");
    int32_t ret = proxy_->SetPortRole(portId, powerRole, dataRole);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s failed ret = %{public}d!", __func__, ret);
    }
    return ret;
}

int32_t UsbSrvClient::ClaimInterface(USBDevicePipe &pipe, const UsbInterface &interface, bool force)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    int32_t ret = proxy_->ClaimInterface(pipe.GetBusNum(), pipe.GetDevAddr(), interface.GetId());
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s : failed width ret = %{public}d !", __func__, ret);
    }
    return ret;
}
int32_t UsbSrvClient::ReleaseInterface(USBDevicePipe &pipe, const UsbInterface &interface)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    int32_t ret = proxy_->ReleaseInterface(pipe.GetBusNum(), pipe.GetDevAddr(), interface.GetId());
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s : failed width ret = %{public}d !", __func__, ret);
    }
    return ret;
}
int32_t UsbSrvClient::BulkTransfer(USBDevicePipe &pipe,
                                   const USBEndpoint &endpoint,
                                   std::vector<uint8_t> &vdata,
                                   int32_t timeout)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    int32_t ret = ERR_INVALID_VALUE;
    const UsbDev tdev = {pipe.GetBusNum(), pipe.GetDevAddr()};
    const UsbPipe tpipe = {endpoint.GetInterfaceId(), endpoint.GetAddress()};
    if (USB_ENDPOINT_DIR_IN == endpoint.GetDirection()) {
        ret = proxy_->BulkTransferRead(tdev, tpipe, vdata, timeout);
    } else if (USB_ENDPOINT_DIR_OUT == endpoint.GetDirection()) {
        ret = proxy_->BulkTransferWrite(tdev, tpipe, vdata, timeout);
    }
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s : failed width ret = %{public}d !", __func__, ret);
    }
    return ret;
}
int32_t UsbSrvClient::ControlTransfer(USBDevicePipe &pipe, const UsbCtrlTransfer &ctrl, std::vector<uint8_t> &vdata)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    const UsbDev dev = {pipe.GetBusNum(), pipe.GetDevAddr()};
    int32_t ret = proxy_->ControlTransfer(dev, ctrl, vdata);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "%{public}s : failed width ret = %{public}d !", __func__, ret);
    }

    return ret;
}
int32_t UsbSrvClient::SetConfiguration(USBDevicePipe &pipe, const USBConfig &config)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    int32_t ret = proxy_->SetActiveConfig(pipe.GetBusNum(), pipe.GetDevAddr(), config.GetId());
    return ret;
}
int32_t UsbSrvClient::SetInterface(USBDevicePipe &pipe, const UsbInterface &interface)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    return proxy_->SetInterface(pipe.GetBusNum(), pipe.GetDevAddr(), interface.GetId(),
                                interface.GetAlternateSetting());
}
int32_t UsbSrvClient::GetRawDescriptors(std::vector<uint8_t> &vdata)
{
    return UEC_OK;
}
int32_t UsbSrvClient::GetFileDescriptor()
{
    return UEC_OK;
}

bool UsbSrvClient::Close(const USBDevicePipe &pipe)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, false);
    int32_t ret = proxy_->Close(pipe.GetBusNum(), pipe.GetDevAddr());
    return (ret == UEC_OK);
}

int32_t UsbSrvClient::PipeRequestWait(USBDevicePipe &pipe, int64_t timeout, UsbRequest &req)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    std::vector<uint8_t> cData;
    std::vector<uint8_t> vData;
    const UsbDev tdev = {pipe.GetBusNum(), pipe.GetDevAddr()};
    int32_t ret = proxy_->RequestWait(tdev, timeout, cData, vData);
    if (ret != UEC_OK) {
        USB_HILOGI(MODULE_USB_INNERKIT, "UsbSrvClient::%{public}s:%{public}d :failed width ret = %{public}d.", __func__,
                   __LINE__, ret);
        return ret;
    }

    req.SetPipe(pipe);
    req.SetClientData(cData);
    req.SetReqData(vData);

    PrintBuffer("UsbSrvClient::PipeRequestWait ClientData", (const uint8_t *)cData.data(), cData.size());
    PrintBuffer("UsbSrvClient::PipeRequestWait Buffer", (const uint8_t *)vData.data(), vData.size());
    return ret;
}

int32_t UsbSrvClient::RequestInitialize(UsbRequest &request)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    const USBDevicePipe &pipe = request.GetPipe();
    const USBEndpoint &endpoint = request.GetEndpoint();
    return proxy_->ClaimInterface(pipe.GetBusNum(), pipe.GetDevAddr(), endpoint.GetInterfaceId());
}

int32_t UsbSrvClient::RequestFree(UsbRequest &request)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    const USBDevicePipe &pipe = request.GetPipe();
    const USBEndpoint &ep = request.GetEndpoint();
    return proxy_->ReleaseInterface(pipe.GetBusNum(), pipe.GetDevAddr(), ep.GetInterfaceId());
}

int32_t UsbSrvClient::RequestAbort(UsbRequest &request)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    const USBDevicePipe &pipe = request.GetPipe();
    const USBEndpoint &ep = request.GetEndpoint();
    return proxy_->RequestCancel(pipe.GetBusNum(), pipe.GetDevAddr(), ep.GetInterfaceId(), ep.GetAddress());
}

int32_t UsbSrvClient::RequestQueue(UsbRequest &request)
{
    RETURN_IF_WITH_RET(proxy_ == nullptr, UEC_INTERFACE_NO_INIT);
    const USBDevicePipe &pipe = request.GetPipe();
    const USBEndpoint &ep = request.GetEndpoint();
    const UsbDev tdev = {pipe.GetBusNum(), pipe.GetDevAddr()};
    const UsbPipe tpipe = {ep.GetInterfaceId(), ep.GetAddress()};
    return proxy_->RequestQueue(tdev, tpipe, request.GetClientData(), request.GetReqData());
}
} // namespace USB
} // namespace OHOS
