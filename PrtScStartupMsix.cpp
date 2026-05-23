#include <roapi.h>
#include <windows.h>

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.h>

namespace
{
constexpr wchar_t kStartupTaskId[] = L"PrtScStartupTask";

class RoApartment
{
public:
    RoApartment()
    {
        const HRESULT result = RoInitialize(RO_INIT_MULTITHREADED);
        shouldUninitialize_ = result == S_OK || result == S_FALSE;
    }

    ~RoApartment()
    {
        if (shouldUninitialize_)
        {
            RoUninitialize();
        }
    }

private:
    bool shouldUninitialize_ = false;
};

winrt::Windows::ApplicationModel::StartupTask GetStartupTask()
{
    RoApartment apartment;
    return winrt::Windows::ApplicationModel::StartupTask::GetAsync(kStartupTaskId).get();
}

bool IsEnabledState(winrt::Windows::ApplicationModel::StartupTaskState state)
{
    using winrt::Windows::ApplicationModel::StartupTaskState;
    return state == StartupTaskState::Enabled || state == StartupTaskState::EnabledByPolicy;
}
}

extern "C" __declspec(dllexport) BOOL WINAPI PrtScStartupIsSupported()
{
    try
    {
        GetStartupTask();
        return TRUE;
    }
    catch (...)
    {
        return FALSE;
    }
}

extern "C" __declspec(dllexport) BOOL WINAPI PrtScStartupIsEnabled(BOOL* enabled)
{
    if (enabled == nullptr)
    {
        return FALSE;
    }

    try
    {
        const auto task = GetStartupTask();
        *enabled = IsEnabledState(task.State()) ? TRUE : FALSE;
        return TRUE;
    }
    catch (...)
    {
        *enabled = FALSE;
        return FALSE;
    }
}

extern "C" __declspec(dllexport) BOOL WINAPI PrtScStartupSetEnabled(BOOL enabled)
{
    try
    {
        const auto task = GetStartupTask();
        if (enabled == FALSE)
        {
            task.Disable();
            return TRUE;
        }

        const auto state = task.RequestEnableAsync().get();
        return IsEnabledState(state) ? TRUE : FALSE;
    }
    catch (...)
    {
        return FALSE;
    }
}
