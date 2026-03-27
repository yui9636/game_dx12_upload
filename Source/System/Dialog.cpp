#include "Dialog.h"
#include <shobjidl.h>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>

namespace {
    static char pathBuffer[MAX_PATH] = {};
    const GUID kOpenDialogClientGuid =
    { 0x1e20d5c1, 0x2d34, 0x4b7a,{ 0x8c, 0x6c, 0x91, 0x33, 0x52, 0x4e, 0x11, 0xa1 } };
    const GUID kSaveDialogClientGuid =
    { 0x7b26ef14, 0x0f37, 0x46b2,{ 0x9b, 0x6b, 0x43, 0x8c, 0x8f, 0x31, 0x2d, 0x44 } };

    std::wstring ToWide(const char* text)
    {
        if (text == nullptr || text[0] == '\0') {
            return {};
        }
        const int required = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (required <= 0) {
            return {};
        }
        std::wstring out(static_cast<size_t>(required - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), required);
        return out;
    }

    std::string ToUtf8(const wchar_t* text)
    {
        if (text == nullptr || text[0] == L'\0') {
            return {};
        }
        const int required = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (required <= 0) {
            return {};
        }
        std::string out(static_cast<size_t>(required - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), required, nullptr, nullptr);
        return out;
    }

    std::filesystem::path ResolveInitialDirectory(const char* filepath)
    {
        if (filepath != nullptr && filepath[0] != '\0') {
            std::filesystem::path path(filepath);
            if (path.has_filename()) {
                path = path.parent_path();
            }
            if (!path.empty()) {
                return std::filesystem::absolute(path);
            }
        }
        if (pathBuffer[0] != '\0') {
            std::filesystem::path path(pathBuffer);
            if (path.has_filename()) {
                path = path.parent_path();
            }
            if (!path.empty()) {
                return std::filesystem::absolute(path);
            }
        }
        return {};
    }

    std::vector<std::wstring> BuildFilterStorage(const char* filter)
    {
        std::vector<std::wstring> storage;
        if (filter == nullptr) {
            storage.emplace_back(L"All Files");
            storage.emplace_back(L"*.*");
            return storage;
        }

        const char* p = filter;
        while (*p != '\0') {
            std::wstring label = ToWide(p);
            p += std::strlen(p) + 1;
            if (*p == '\0') {
                break;
            }
            std::wstring spec = ToWide(p);
            p += std::strlen(p) + 1;
            storage.emplace_back(std::move(label));
            storage.emplace_back(std::move(spec));
        }
        if (storage.empty()) {
            storage.emplace_back(L"All Files");
            storage.emplace_back(L"*.*");
        }
        return storage;
    }

    std::vector<COMDLG_FILTERSPEC> BuildFilterSpecs(std::vector<std::wstring>& storage)
    {
        std::vector<COMDLG_FILTERSPEC> specs;
        for (size_t i = 0; i + 1 < storage.size(); i += 2) {
            specs.push_back(COMDLG_FILTERSPEC{ storage[i].c_str(), storage[i + 1].c_str() });
        }
        return specs;
    }

    void ApplyInitialDirectory(IFileDialog* dialog, const char* filepath)
    {
        const std::filesystem::path initialDir = ResolveInitialDirectory(filepath);
        if (initialDir.empty()) {
            return;
        }
        const std::wstring wideDir = initialDir.wstring();
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(wideDir.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }
    }

    DialogResult FinalizeDialogResult(const std::string& selectedPath, char* filepath, int size)
    {
        if (selectedPath.empty()) {
            return DialogResult::Cancel;
        }
        strcpy_s(filepath, static_cast<size_t>(size), selectedPath.c_str());
        strcpy_s(pathBuffer, MAX_PATH, selectedPath.c_str());
        return DialogResult::OK;
    }
}

DialogResult Dialog::OpenFileName(char* filepath, int size, const char* filter, const char* title, HWND hWnd, bool multiSelect)
{
    HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool ownsCom = SUCCEEDED(initHr);

    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || dialog == nullptr) {
        if (ownsCom) CoUninitialize();
        return DialogResult::Cancel;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
    if (multiSelect) {
        options |= FOS_ALLOWMULTISELECT;
    }
    dialog->SetOptions(options);

    if (title != nullptr) {
        const std::wstring wideTitle = ToWide(title);
        if (!wideTitle.empty()) {
            dialog->SetTitle(wideTitle.c_str());
        }
    }
    dialog->SetClientGuid(kOpenDialogClientGuid);
    dialog->ClearClientData();

    auto filterStorage = BuildFilterStorage(filter);
    auto specs = BuildFilterSpecs(filterStorage);
    if (!specs.empty()) {
        dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
        dialog->SetFileTypeIndex(1);
    }

    ApplyInitialDirectory(dialog, filepath);

    hr = dialog->Show(hWnd);
    if (FAILED(hr)) {
        dialog->Release();
        if (ownsCom) CoUninitialize();
        return DialogResult::Cancel;
    }

    IShellItem* result = nullptr;
    hr = dialog->GetResult(&result);
    if (FAILED(hr) || result == nullptr) {
        dialog->Release();
        if (ownsCom) CoUninitialize();
        return DialogResult::Cancel;
    }

    PWSTR widePath = nullptr;
    hr = result->GetDisplayName(SIGDN_FILESYSPATH, &widePath);
    std::string selectedPath;
    if (SUCCEEDED(hr) && widePath != nullptr) {
        selectedPath = ToUtf8(widePath);
        CoTaskMemFree(widePath);
    }

    result->Release();
    dialog->Release();
    if (ownsCom) CoUninitialize();
    return FinalizeDialogResult(selectedPath, filepath, size);
}

DialogResult Dialog::SaveFileName(char* filepath, int size, const char* filter, const char* title, const char* ext, HWND hWnd)
{
    HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool ownsCom = SUCCEEDED(initHr);

    IFileSaveDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || dialog == nullptr) {
        if (ownsCom) CoUninitialize();
        return DialogResult::Cancel;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT;
    dialog->SetOptions(options);

    if (title != nullptr) {
        const std::wstring wideTitle = ToWide(title);
        if (!wideTitle.empty()) {
            dialog->SetTitle(wideTitle.c_str());
        }
    }
    dialog->SetClientGuid(kSaveDialogClientGuid);
    dialog->ClearClientData();

    auto filterStorage = BuildFilterStorage(filter);
    auto specs = BuildFilterSpecs(filterStorage);
    if (!specs.empty()) {
        dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
        dialog->SetFileTypeIndex(1);
    }

    if (ext != nullptr && ext[0] != '\0') {
        const std::wstring wideExt = ToWide(ext);
        if (!wideExt.empty()) {
            dialog->SetDefaultExtension(wideExt.c_str());
        }
    }

    ApplyInitialDirectory(dialog, filepath);

    if (filepath != nullptr && filepath[0] != '\0') {
        const std::filesystem::path path(filepath);
        const std::wstring filename = path.filename().wstring();
        if (!filename.empty()) {
            dialog->SetFileName(filename.c_str());
        }
    }

    hr = dialog->Show(hWnd);
    if (FAILED(hr)) {
        dialog->Release();
        if (ownsCom) CoUninitialize();
        return DialogResult::Cancel;
    }

    IShellItem* result = nullptr;
    hr = dialog->GetResult(&result);
    if (FAILED(hr) || result == nullptr) {
        dialog->Release();
        if (ownsCom) CoUninitialize();
        return DialogResult::Cancel;
    }

    PWSTR widePath = nullptr;
    hr = result->GetDisplayName(SIGDN_FILESYSPATH, &widePath);
    std::string selectedPath;
    if (SUCCEEDED(hr) && widePath != nullptr) {
        selectedPath = ToUtf8(widePath);
        CoTaskMemFree(widePath);
    }

    result->Release();
    dialog->Release();
    if (ownsCom) CoUninitialize();
    return FinalizeDialogResult(selectedPath, filepath, size);
}
