#include "effects/presets/effectpresetmanager.h"

#include <QDir>

#include "effects/backends/effectsbackendmanager.h"
#include "effects/presets/effectxmlelements.h"

namespace {
const QString kEffectDefaultsDirectory = "/effects/defaults";
}

EffectPresetManager::EffectPresetManager(UserSettingsPointer pConfig,
        EffectsBackendManagerPointer pBackendManager)
        : m_pConfig(pConfig),
          m_pBackendManager(pBackendManager) {
    loadDefaultEffectPresets();
}

EffectPresetManager::~EffectPresetManager() {
    for (const auto pEffectPreset : m_defaultPresets) {
        saveDefaultForEffect(pEffectPreset);
    }
}

void EffectPresetManager::loadDefaultEffectPresets() {
    // Load saved defaults from settings directory
    QString dirPath(m_pConfig->getSettingsPath() + kEffectDefaultsDirectory);
    QDir effectsDefaultsDir(dirPath);
    effectsDefaultsDir.setFilter(QDir::Files | QDir::Readable);
    for (const auto& filePath : effectsDefaultsDir.entryList()) {
        QFile file(dirPath + "/" + filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QDomDocument doc;
        if (!doc.setContent(&file)) {
            file.close();
            continue;
        }
        EffectPresetPointer pEffectPreset(new EffectPreset(doc.documentElement()));
        if (!pEffectPreset->isEmpty()) {
            EffectManifestPointer pManifest = m_pBackendManager->getManifest(
                    pEffectPreset->id(), pEffectPreset->backendType());
            m_defaultPresets.insert(pManifest, pEffectPreset);
        }
        file.close();
    }

    // If no preset was found, generate one from the manifest
    for (const auto& pManifest : m_pBackendManager->getManifests()) {
        if (!m_defaultPresets.contains(pManifest)) {
            m_defaultPresets.insert(pManifest,
                    EffectPresetPointer(new EffectPreset(pManifest)));
        }
    }
}

void EffectPresetManager::saveDefaultForEffect(EffectPresetPointer pEffectPreset) {
    if (pEffectPreset->isEmpty()) {
        return;
    }

    const auto pManifest = m_pBackendManager->getManifest(
            pEffectPreset->id(), pEffectPreset->backendType());
    m_defaultPresets.insert(pManifest, pEffectPreset);

    QDomDocument doc(EffectXml::Effect);
    doc.setContent(EffectXml::FileHeader);
    doc.appendChild(pEffectPreset->toXml(&doc));

    QString path(m_pConfig->getSettingsPath() + kEffectDefaultsDirectory);
    QDir effectsDefaultsDir(path);
    if (!effectsDefaultsDir.exists()) {
        effectsDefaultsDir.mkpath(path);
    }

    // The file name does not matter as long as it is unique. The actual id string
    // is safely stored in the UTF8 document, regardless of what the filesystem
    // supports for file names.
    QString fileName = pEffectPreset->id();
    // LV2 ids are URLs
    fileName.replace("/", "-");
    QStringList forbiddenCharacters;
    forbiddenCharacters << "<"
                        << ">"
                        << ":"
                        << "\""
                        << "\'"
                        << "|"
                        << "?"
                        << "*"
                        << "\\";
    for (const auto& character : forbiddenCharacters) {
        fileName.remove(character);
    }
    QFile file(path + "/" + fileName + ".xml");
    if (!file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        return;
    }
    file.write(doc.toString().toUtf8());
    file.close();
}
