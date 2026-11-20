#pragma once
#include <QString>
#include <QStringList>

namespace UbiqueDShow {

// Enumerate all connected Windows DirectShow video capture devices
QStringList listCameras();

// Read all IAMVideoProcAmp and IAMCameraControl properties as compact JSON
QString captureCameraSettingsJson(const QString &deviceName, QString *errOut = nullptr);

// Apply saved JSON properties back to the DirectShow camera
bool applyCameraSettingsJson(const QString &deviceName, const QString &jsonStr, QString *errOut = nullptr);

} // namespace UbiqueDShow
