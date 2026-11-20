#include "ubique-dshow.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dshow.h>

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#include <QJsonDocument>
#include <QJsonObject>

namespace UbiqueDShow {

struct ComScope {
    bool needUninit = false;
    ComScope() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(hr)) needUninit = true;
    }
    ~ComScope() {
        if (needUninit) CoUninitialize();
    }
};

static IBaseFilter *findCameraFilter(const QString &targetName)
{
    ICreateDevEnum *devEnum = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ICreateDevEnum, (void **)&devEnum);
    if (FAILED(hr) || !devEnum) return nullptr;

    IEnumMoniker *enumMoniker = nullptr;
    hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMoniker, 0);
    if (hr != S_OK || !enumMoniker) {
        devEnum->Release();
        return nullptr;
    }

    IBaseFilter *result = nullptr;
    IMoniker *moniker = nullptr;
    while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
        IPropertyBag *propBag = nullptr;
        hr = moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag, (void **)&propBag);
        if (SUCCEEDED(hr) && propBag) {
            VARIANT varName;
            VariantInit(&varName);
            if (SUCCEEDED(propBag->Read(L"FriendlyName", &varName, nullptr)) && varName.vt == VT_BSTR) {
                QString name = QString::fromWCharArray(varName.bstrVal);
                if (name == targetName) {
                    moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void **)&result);
                }
            }
            VariantClear(&varName);
            propBag->Release();
        }
        moniker->Release();
        if (result) break;
    }

    enumMoniker->Release();
    devEnum->Release();
    return result;
}

QStringList listCameras()
{
    QStringList list;
    ComScope com;

    ICreateDevEnum *devEnum = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ICreateDevEnum, (void **)&devEnum);
    if (FAILED(hr) || !devEnum) return list;

    IEnumMoniker *enumMoniker = nullptr;
    hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMoniker, 0);
    if (hr != S_OK || !enumMoniker) {
        devEnum->Release();
        return list;
    }

    IMoniker *moniker = nullptr;
    while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
        IPropertyBag *propBag = nullptr;
        if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag, (void **)&propBag)) && propBag) {
            VARIANT varName;
            VariantInit(&varName);
            if (SUCCEEDED(propBag->Read(L"FriendlyName", &varName, nullptr)) && varName.vt == VT_BSTR) {
                QString name = QString::fromWCharArray(varName.bstrVal);
                if (!name.isEmpty() && !list.contains(name)) {
                    list.append(name);
                }
            }
            VariantClear(&varName);
            propBag->Release();
        }
        moniker->Release();
    }

    enumMoniker->Release();
    devEnum->Release();
    return list;
}

static const long kProcAmpProps[] = {
    VideoProcAmp_Brightness,
    VideoProcAmp_Contrast,
    VideoProcAmp_Hue,
    VideoProcAmp_Saturation,
    VideoProcAmp_Sharpness,
    VideoProcAmp_Gamma,
    VideoProcAmp_ColorEnable,
    VideoProcAmp_WhiteBalance,
    VideoProcAmp_BacklightCompensation,
    VideoProcAmp_Gain
};

static const long kCamCtrlProps[] = {
    CameraControl_Pan,
    CameraControl_Tilt,
    CameraControl_Roll,
    CameraControl_Zoom,
    CameraControl_Exposure,
    CameraControl_Iris,
    CameraControl_Focus
};

QString captureCameraSettingsJson(const QString &deviceName, QString *errOut)
{
    if (deviceName.isEmpty()) {
        if (errOut) *errOut = "Камера не выбрана";
        return QString();
    }

    ComScope com;
    IBaseFilter *cam = findCameraFilter(deviceName);
    if (!cam) {
        if (errOut) *errOut = "Камера не найдена: " + deviceName;
        return QString();
    }

    QJsonObject root;
    QJsonObject procObj;
    QJsonObject ctrlObj;
    int savedCount = 0;

    IAMVideoProcAmp *procAmp = nullptr;
    if (SUCCEEDED(cam->QueryInterface(IID_IAMVideoProcAmp, (void **)&procAmp)) && procAmp) {
        for (long prop : kProcAmpProps) {
            long val = 0, flags = 0;
            if (SUCCEEDED(procAmp->Get(prop, &val, &flags))) {
                QJsonObject p;
                p["v"] = (int)val;
                p["f"] = (int)flags;
                procObj[QString::number(prop)] = p;
                savedCount++;
            }
        }
        procAmp->Release();
    }

    IAMCameraControl *camCtrl = nullptr;
    if (SUCCEEDED(cam->QueryInterface(IID_IAMCameraControl, (void **)&camCtrl)) && camCtrl) {
        for (long prop : kCamCtrlProps) {
            long val = 0, flags = 0;
            if (SUCCEEDED(camCtrl->Get(prop, &val, &flags))) {
                QJsonObject p;
                p["v"] = (int)val;
                p["f"] = (int)flags;
                ctrlObj[QString::number(prop)] = p;
                savedCount++;
            }
        }
        camCtrl->Release();
    }

    cam->Release();

    if (savedCount == 0) {
        if (errOut) *errOut = "Драйвер камеры не вернул параметры";
        return QString();
    }

    root["proc"] = procObj;
    root["ctrl"] = ctrlObj;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool applyCameraSettingsJson(const QString &deviceName, const QString &jsonStr, QString *errOut)
{
    if (deviceName.isEmpty()) {
        if (errOut) *errOut = "Камера не выбрана";
        return false;
    }
    if (jsonStr.isEmpty()) {
        if (errOut) *errOut = "Слот пуст — сначала сохрани настройки";
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) {
        if (errOut) *errOut = "Повреждённые данные пресета";
        return false;
    }

    ComScope com;
    IBaseFilter *cam = findCameraFilter(deviceName);
    if (!cam) {
        if (errOut) *errOut = "Камера не найдена: " + deviceName;
        return false;
    }

    QJsonObject root = doc.object();
    QJsonObject procObj = root["proc"].toObject();
    QJsonObject ctrlObj = root["ctrl"].toObject();
    int appliedCount = 0;

    IAMVideoProcAmp *procAmp = nullptr;
    if (SUCCEEDED(cam->QueryInterface(IID_IAMVideoProcAmp, (void **)&procAmp)) && procAmp) {
        for (auto it = procObj.begin(); it != procObj.end(); ++it) {
            long prop = it.key().toLong();
            QJsonObject p = it.value().toObject();
            long val = (long)p["v"].toInt();
            long flags = (long)p["f"].toInt();
            if (SUCCEEDED(procAmp->Set(prop, val, flags))) {
                appliedCount++;
            }
        }
        procAmp->Release();
    }

    IAMCameraControl *camCtrl = nullptr;
    if (SUCCEEDED(cam->QueryInterface(IID_IAMCameraControl, (void **)&camCtrl)) && camCtrl) {
        for (auto it = ctrlObj.begin(); it != ctrlObj.end(); ++it) {
            long prop = it.key().toLong();
            QJsonObject p = it.value().toObject();
            long val = (long)p["v"].toInt();
            long flags = (long)p["f"].toInt();
            if (SUCCEEDED(camCtrl->Set(prop, val, flags))) {
                appliedCount++;
            }
        }
        camCtrl->Release();
    }

    cam->Release();
    if (appliedCount == 0) {
        if (errOut) *errOut = "Не удалось применить параметры к камере";
        return false;
    }
    return true;
}

} // namespace UbiqueDShow

#else
namespace UbiqueDShow {
QStringList listCameras() { return {}; }
QString captureCameraSettingsJson(const QString &, QString *errOut) {
    if (errOut) *errOut = "Только для Windows";
    return {};
}
bool applyCameraSettingsJson(const QString &, const QString &, QString *errOut) {
    if (errOut) *errOut = "Только для Windows";
    return false;
}
} // namespace UbiqueDShow
#endif
