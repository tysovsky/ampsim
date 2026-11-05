#include "Maya44Inputs.h"

#if defined (_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif

#include <windows.h>
#include <setupapi.h>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Maya44Inputs
{
namespace
{
    // The device interface that the card's control driver registers on the card's PCI device.
    // ESI's control panel and ASIO driver both find the card through it.
    const GUID controlInterface = { 0x66d25bb5, 0xac71, 0x4c82, { 0xa6, 0x05, 0x70, 0xc2, 0x19, 0x2a, 0xee, 0x82 } };

    // Every command goes through this one IOCTL (buffered, any access) as a 128-byte packet,
    // which the driver answers in place: the command's group at byte 0x00, the command at
    // byte 0x08, and its arguments after that.
    constexpr DWORD commandIoctl = 0x2A3BB8;

    // Group 1, command 1: identify. The driver answers with "MAYA" at 0x0C and "eX  " at 0x10.
    constexpr uint32_t maya = 0x4159414D, eX = 0x20205865;

    // Group 3 works on the settings block, whose address goes at 0x10. Command 1 copies the
    // driver's block there. Command 10 takes the block from there, applies what differs from
    // the driver's own and saves it in the driver's registry key ("MixerMA44XTe"), which the
    // driver loads when it starts. (Command 2 does the same without saving.)
    constexpr uint32_t settingsGroup = 3, getSettings = 1, applyAndSaveSettings = 10;

    // The settings block, and where the input modes are in it
    constexpr size_t settingsSize = 0x21C;
    constexpr size_t input12Offset = 0x64, input34Offset = 0x68;

    struct Packet
    {
        Packet (uint32_t group, uint32_t command)
        {
            put (0x00, group);
            put (0x08, command);
        }

        void put (size_t offset, uint32_t value)        { std::memcpy (bytes + offset, &value, sizeof (value)); }
        void putAddress (size_t offset, void* address)  { std::memcpy (bytes + offset, &address, sizeof (address)); }

        uint32_t get (size_t offset) const
        {
            uint32_t value;
            std::memcpy (&value, bytes + offset, sizeof (value));
            return value;
        }

        alignas (8) unsigned char bytes[128] = {};
    };

    uint32_t getSetting (const std::vector<unsigned char>& settings, size_t offset)
    {
        uint32_t value;
        std::memcpy (&value, settings.data() + offset, sizeof (value));
        return value;
    }

    //==============================================================================
    // A handle to the first MAYA44 eX control driver found, closed when this goes
    class Card
    {
    public:
        Card()
        {
            auto devices = SetupDiGetClassDevsW (&controlInterface, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

            if (devices == INVALID_HANDLE_VALUE)
                return;

            SP_DEVICE_INTERFACE_DATA interfaceData {};
            interfaceData.cbSize = sizeof (interfaceData);

            for (DWORD i = 0; handle == INVALID_HANDLE_VALUE
                              && SetupDiEnumDeviceInterfaces (devices, nullptr, &controlInterface, i, &interfaceData); ++i)
            {
                DWORD size = 0;
                SetupDiGetDeviceInterfaceDetailW (devices, &interfaceData, nullptr, 0, &size, nullptr);

                if (size < sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA_W))
                    continue;

                std::vector<unsigned char> detailBuffer (size);
                auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*> (detailBuffer.data());
                detail->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA_W);

                if (! SetupDiGetDeviceInterfaceDetailW (devices, &interfaceData, detail, size, nullptr, nullptr))
                    continue;

                handle = CreateFileW (detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      nullptr, OPEN_EXISTING, 0, nullptr);

                if (handle != INVALID_HANDLE_VALUE && ! isMaya44eX())
                    close();
            }

            SetupDiDestroyDeviceInfoList (devices);
        }

        ~Card()                         { close(); }

        bool isOpen() const             { return handle != INVALID_HANDLE_VALUE; }

        bool send (Packet& packet)
        {
            DWORD bytesReturned = 0;
            return DeviceIoControl (handle, commandIoctl, packet.bytes, sizeof (packet.bytes),
                                    packet.bytes, sizeof (packet.bytes), &bytesReturned, nullptr) != FALSE;
        }

        std::optional<std::vector<unsigned char>> readSettings()
        {
            std::vector<unsigned char> settings (settingsSize);
            Packet packet (settingsGroup, getSettings);
            packet.putAddress (0x10, settings.data());

            if (! send (packet))
                return {};

            return settings;
        }

        bool writeSettings (std::vector<unsigned char>& settings)
        {
            Packet packet (settingsGroup, applyAndSaveSettings);
            packet.putAddress (0x10, settings.data());
            return send (packet);
        }

    private:
        bool isMaya44eX()
        {
            // The same question ESI's panel asks, with the same placeholders in the answer's place
            Packet packet (1, 1);
            packet.put (0x0C, 0xFF);
            packet.put (0x10, 0xFF);

            return send (packet) && packet.get (0x0C) == maya && packet.get (0x10) == eX;
        }

        void close()
        {
            if (handle != INVALID_HANDLE_VALUE)
                CloseHandle (handle);

            handle = INVALID_HANDLE_VALUE;
        }

        HANDLE handle = INVALID_HANDLE_VALUE;
    };

    bool setSetting (size_t offset, uint32_t value)
    {
        Card card;

        if (! card.isOpen())
            return false;

        // Start from the driver's own block, so nothing else in it changes
        auto settings = card.readSettings();

        if (! settings.has_value())
            return false;

        if (getSetting (*settings, offset) == value)
            return true;

        std::memcpy (settings->data() + offset, &value, sizeof (value));
        return card.writeSettings (*settings);
    }
}

//==============================================================================
bool isAvailable()
{
    return Card().isOpen();
}

std::optional<Modes> read()
{
    Card card;

    if (! card.isOpen())
        return {};

    auto settings = card.readSettings();

    if (! settings.has_value())
        return {};

    const auto input12 = getSetting (*settings, input12Offset);
    const auto input34 = getSetting (*settings, input34Offset);

    if (input12 > (uint32_t) Input12Mode::digital || input34 > (uint32_t) Input34Mode::hiZ)
        return {};

    return Modes { (Input12Mode) input12, (Input34Mode) input34 };
}

bool setInput12Mode (Input12Mode mode)  { return setSetting (input12Offset, (uint32_t) mode); }
bool setInput34Mode (Input34Mode mode)  { return setSetting (input34Offset, (uint32_t) mode); }
}

#else

namespace Maya44Inputs
{
    bool isAvailable()                      { return false; }
    std::optional<Modes> read()             { return {}; }
    bool setInput12Mode (Input12Mode)       { return false; }
    bool setInput34Mode (Input34Mode)       { return false; }
}

#endif
