#include <filesystem>
#include <string>
#include <thread>
#include <vector>
#include <objbase.h>
#include <objidl.h>
#include <windows.h>
#include <gdiplus.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/base.h>

namespace
{
bool GetPngEncoderClsid(CLSID& clsid)
{
    UINT encoderCount = 0;
    UINT encoderBytes = 0;
    if (Gdiplus::GetImageEncodersSize(&encoderCount, &encoderBytes) != Gdiplus::Ok || encoderBytes == 0)
    {
        return false;
    }

    std::vector<BYTE> buffer(encoderBytes);
    auto* encoders = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(encoderCount, encoderBytes, encoders) != Gdiplus::Ok)
    {
        return false;
    }

    for (UINT i = 0; i < encoderCount; ++i)
    {
        if (wcscmp(encoders[i].MimeType, L"image/png") == 0)
        {
            clsid = encoders[i].Clsid;
            return true;
        }
    }

    return false;
}

std::filesystem::path MakeTempPngPath()
{
    wchar_t tempDirectory[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, tempDirectory) == 0)
    {
        return {};
    }

    wchar_t tempFile[MAX_PATH]{};
    if (GetTempFileNameW(tempDirectory, L"pts", 0, tempFile) == 0)
    {
        return {};
    }

    std::filesystem::path path = tempFile;
    DeleteFileW(path.c_str());
    path.replace_extension(L".png");
    return path;
}

bool SaveBitmapAsPng(HBITMAP bitmap, const std::filesystem::path& path)
{
    Gdiplus::GdiplusStartupInput startupInput{};
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok)
    {
        return false;
    }

    CLSID pngClsid{};
    const bool hasPngEncoder = GetPngEncoderClsid(pngClsid);
    bool saved = false;
    if (hasPngEncoder)
    {
        Gdiplus::Bitmap gdiplusBitmap(bitmap, nullptr);
        saved = gdiplusBitmap.Save(path.wstring().c_str(), &pngClsid, nullptr) == Gdiplus::Ok;
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return saved;
}

std::wstring ReadTextFromResult(const winrt::Windows::Media::Ocr::OcrResult& result)
{
    std::wstring text;
    for (const auto& line : result.Lines())
    {
        if (!text.empty())
        {
            text += L"\r\n";
        }

        text += line.Text().c_str();
    }

    return text;
}

bool RecognizeBitmapTextOnCurrentThread(HBITMAP bitmap, std::wstring& text)
{
    text.clear();
    if (bitmap == nullptr)
    {
        return false;
    }

    const std::filesystem::path tempPath = MakeTempPngPath();
    if (tempPath.empty() || !SaveBitmapAsPng(bitmap, tempPath))
    {
        return false;
    }

    bool recognized = false;
    bool apartmentInitialized = false;
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        apartmentInitialized = true;

        auto ocrEngine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();
        if (ocrEngine != nullptr)
        {
            auto file = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(tempPath.c_str()).get();
            auto stream = file.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read).get();
            auto decoder = winrt::Windows::Graphics::Imaging::BitmapDecoder::CreateAsync(stream).get();
            auto softwareBitmap = decoder.GetSoftwareBitmapAsync(
                winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
                winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied).get();
            auto result = ocrEngine.RecognizeAsync(softwareBitmap).get();

            text = ReadTextFromResult(result);
            recognized = true;
        }
    }
    catch (...)
    {
        recognized = false;
        text.clear();
    }

    DeleteFileW(tempPath.c_str());
    if (apartmentInitialized)
    {
        winrt::uninit_apartment();
    }
    return recognized;
}

bool RecognizeBitmapText(HBITMAP bitmap, std::wstring& text)
{
    bool recognized = false;
    std::wstring recognizedText;

    std::thread worker([&]()
    {
        recognized = RecognizeBitmapTextOnCurrentThread(bitmap, recognizedText);
    });
    worker.join();

    text = std::move(recognizedText);
    return recognized;
}

PWSTR CopyTextForCaller(const std::wstring& text)
{
    const size_t byteCount = (text.size() + 1) * sizeof(wchar_t);
    auto* result = static_cast<PWSTR>(CoTaskMemAlloc(byteCount));
    if (result == nullptr)
    {
        return nullptr;
    }

    wcscpy_s(result, text.size() + 1, text.c_str());
    return result;
}
}

extern "C" __declspec(dllexport) BOOL WINAPI PrtScOcrRecognizeBitmapText(HBITMAP bitmap, PWSTR* recognizedText)
{
    if (recognizedText == nullptr)
    {
        return FALSE;
    }

    *recognizedText = nullptr;

    std::wstring text;
    if (!RecognizeBitmapText(bitmap, text))
    {
        return FALSE;
    }

    *recognizedText = CopyTextForCaller(text);
    return *recognizedText != nullptr;
}
