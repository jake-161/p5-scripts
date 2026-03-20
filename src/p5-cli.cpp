#include <P1Camera.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace P1::CameraSdk;

namespace {

struct Options {
    std::string ip;
    std::string command;
    std::string argument;
    std::string argument2;
    std::string argument3;
    std::string password;
    bool help = false;
};

constexpr uint32_t kPropertyStaticSetup = 6023;
constexpr uint32_t kPropertyIpAddress = 6024;
constexpr uint32_t kPropertyNetmask = 6025;
constexpr uint32_t kPropertyGateway = 6026;
constexpr uint32_t kPropertyDhcpEnabled = 6027;
constexpr uint32_t kPropertyNetworkStatusAction = 6029;

std::string PropertyValueSummary(PropertyValue const& value)
{
    if (!value.mPresentationString.empty()) {
        return value.mPresentationString;
    }
    return value.ToString();
}

std::string TryGetCameraModel(Camera const& camera)
{
    try {
        return camera.PropertySpec(2).mValue.StringValue();
    } catch (SdkException const&) {
        return std::string();
    }
}

bool IsP5Camera(Camera const& camera)
{
    auto model = TryGetCameraModel(camera);
    return model.rfind("P5", 0) == 0;
}

bool LooksLikeIpv4Value(std::string const& value)
{
    if (value.empty()) {
        return false;
    }

    return std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0 || c == '.';
    });
}

void PrintUsage(char const* argv0)
{
    std::cerr
        << "P5 CLI\n"
        << "Usage:\n"
        << "  " << argv0 << " -h | --help\n"
        << "  " << argv0 << " <camera-ip> status [password]\n"
        << "  " << argv0 << " <camera-ip> properties [password]\n"
        << "  " << argv0 << " <camera-ip> capture [output.iiq] [password]\n"
        << "  " << argv0 << " <camera-ip> live-view [output.jpg|output.ppm] [password]\n"
        << "  " << argv0 << " <camera-ip> images [password]\n"
        << "  " << argv0 << " <camera-ip> download-image <image-id|filename> [output.iiq] [password]\n"
        << "  " << argv0 << " <camera-ip> property-info <property-id> [password]\n"
        << "  " << argv0 << " <camera-ip> set-property <property-id> <value> [password]\n"
        << "  " << argv0 << " <camera-ip> set-ip <new-ip> [netmask] [gateway] [password]\n"
        << "  " << argv0 << " <camera-ip> set-iso <value> [password]\n"
        << "  " << argv0 << " <camera-ip> set-aperture <value> [password]\n"
        << "  " << argv0 << " <camera-ip> set-shutter-us <microseconds> [password]\n"
        << "  " << argv0 << " <camera-ip> set-shutter <display-value> [password]\n"
        << "  " << argv0 << " <camera-ip> set-exposure-program <value> [password]\n"
        << "  " << argv0 << " <camera-ip> set-host-storage <value> [password]\n"
        << "  " << argv0 << " <camera-ip> set-local-storage <value> [password]\n"
        << "\n"
        << "Commands:\n"
        << "  status                Show key camera state and confirmed subscription support.\n"
        << "  properties            Dump all currently exposed properties.\n"
        << "  capture               Trigger a capture and save the returned IIQ file to host.\n"
        << "  live-view             Subscribe to live view and save the first returned frame.\n"
        << "  images                List images currently visible on camera storage.\n"
        << "  download-image        Download a stored image by image id or filename.\n"
        << "  property-info         Show one property's type, value, range, and allowed values.\n"
        << "  set-property          Set any writable property by numeric id.\n"
        << "  set-ip                Set the camera IP and optionally update netmask and gateway.\n"
        << "  set-iso               Set property 120 (ISO) using an integer value.\n"
        << "  set-aperture          Set property 1003 (Aperture) using a floating point value.\n"
        << "  set-shutter-us        Set property 1012 (Shutter Speed [us]) using an integer value.\n"
        << "  set-shutter           Set property 1002 (Shutter Speed) using the camera display string, for example 1/1000.\n"
        << "  set-exposure-program  Set property 1000, for example Auto or Manual.\n"
        << "  set-host-storage      Set property 1302, for example Off or If Present.\n"
        << "  set-local-storage     Set property 1301, for example Off or If Present.\n"
        << "\n"
        << "Notes:\n"
        << "  Values accepted by set-* commands are camera-defined. If a value is invalid,\n"
        << "  the SDK will reject it or clamp it based on the property specification.\n"
        << "  Use the properties command to inspect current values before changing them.\n"
        << "\n"
        << "Examples:\n"
        << "  " << argv0 << " --help\n"
        << "  " << argv0 << " 192.168.1.241 status\n"
        << "  " << argv0 << " 192.168.1.241 capture shot.iiq\n"
        << "  " << argv0 << " 192.168.1.241 live-view liveview.jpg\n"
        << "  " << argv0 << " 192.168.1.241 images\n"
        << "  " << argv0 << " 192.168.1.241 download-image 42 latest.iiq\n"
        << "  " << argv0 << " 192.168.1.241 property-info 120\n"
        << "  " << argv0 << " 192.168.1.241 set-property 1204 On\n"
        << "  " << argv0 << " 192.168.1.241 set-ip 192.168.1.250\n"
        << "  " << argv0 << " 192.168.1.241 set-ip 192.168.1.250 255.255.255.0 192.168.1.1\n"
        << "  " << argv0 << " 192.168.1.241 set-iso 400\n"
        << "  " << argv0 << " 192.168.1.241 set-shutter-us 1000\n"
        << "  " << argv0 << " 192.168.1.241 set-shutter 1/1000\n";
}

Options ParseArgs(int argc, char** argv)
{
    if (argc == 2) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            Options options;
            options.help = true;
            return options;
        }
    }

    if (argc < 3) {
        PrintUsage(argv[0]);
        throw std::runtime_error("missing required arguments");
    }

    Options options;
    options.ip = argv[1];
    options.command = argv[2];

    if (options.command == "status" || options.command == "properties" || options.command == "images") {
        if (argc >= 4) {
            options.password = argv[3];
        }
        return options;
    }

    if (options.command == "capture") {
        if (argc >= 4) {
            options.argument = argv[3];
        }
        if (argc >= 5) {
            options.password = argv[4];
        }
        return options;
    }

    if (options.command == "live-view") {
        if (argc >= 4) {
            std::string fourth = argv[3];
            if (fourth.find('/') != std::string::npos || fourth.find('.') != std::string::npos) {
                options.argument = fourth;
                if (argc >= 5) {
                    options.password = argv[4];
                }
            } else {
                options.password = fourth;
            }
        }
        return options;
    }

    if (options.command == "property-info" ||
        options.command == "set-property" ||
        options.command == "set-ip" ||
        options.command == "set-iso" ||
        options.command == "set-aperture" ||
        options.command == "set-shutter-us" ||
        options.command == "set-shutter" ||
        options.command == "set-exposure-program" ||
        options.command == "set-host-storage" ||
        options.command == "set-local-storage") {
        if (argc < 4) {
            PrintUsage(argv[0]);
            throw std::runtime_error(options.command + " requires a value");
        }
        options.argument = argv[3];
        if (options.command == "set-property") {
            if (argc < 5) {
                PrintUsage(argv[0]);
                throw std::runtime_error("set-property requires a property id and value");
            }
            options.argument2 = argv[4];
            if (argc >= 6) {
                options.password = argv[5];
            }
        } else if (options.command == "set-ip") {
            if (argc >= 5) {
                std::string fourth = argv[4];
                if (LooksLikeIpv4Value(fourth)) {
                    options.argument2 = fourth;
                    if (argc >= 6) {
                        std::string fifth = argv[5];
                        if (LooksLikeIpv4Value(fifth)) {
                            options.argument3 = fifth;
                            if (argc >= 7) {
                                options.password = argv[6];
                            }
                        } else {
                            options.password = fifth;
                        }
                    }
                } else {
                    options.password = fourth;
                }
            }
        } else if (argc >= 5) {
            options.password = argv[4];
        }
        return options;
    }

    if (options.command == "download-image") {
        if (argc < 4) {
            PrintUsage(argv[0]);
            throw std::runtime_error("download-image requires an image id or filename");
        }
        options.argument = argv[3];
        if (argc >= 5) {
            std::string fourth = argv[4];
            if (fourth.find('/') != std::string::npos || fourth.find('.') != std::string::npos) {
                options.password = argc >= 6 ? argv[5] : std::string();
            } else if (argc == 5) {
                options.password = argv[4];
            }
        }
        return options;
    }

    PrintUsage(argv[0]);
    throw std::runtime_error("unknown command: " + options.command);
}

bool IsNumeric(std::string const& value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

fs::path CaptureOutputPath(IFullImage const& image, std::string const& requestedPath)
{
    if (!requestedPath.empty()) {
        return fs::path(requestedPath);
    }
    if (!image.FileName().empty()) {
        return fs::path(image.FileName());
    }
    return fs::path("capture.iiq");
}

char const* ImageFormatString(ImageFormat format)
{
    switch (format) {
        case ImageFormat::None: return "None";
        case ImageFormat::JPEG: return "JPEG";
        case ImageFormat::RGB24: return "RGB24";
        case ImageFormat::Auto: return "Auto";
        case ImageFormat::IIQ: return "IIQ";
        default: return "Unknown";
    }
}

fs::path LiveViewOutputPath(ILiveView const& image, std::string const& requestedPath)
{
    if (!requestedPath.empty()) {
        return fs::path(requestedPath);
    }
    if (image.Format() == ImageFormat::JPEG) {
        return fs::path("liveview.jpg");
    }
    if (image.Format() == ImageFormat::RGB24) {
        return fs::path("liveview.ppm");
    }
    return fs::path("liveview.bin");
}

void SaveBufferToDisk(std::shared_ptr<uint8_t const> const& data, size_t size, fs::path const& outputPath)
{
    if (!data || size == 0) {
        throw std::runtime_error("image buffer is empty");
    }

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open output file: " + outputPath.string());
    }

    out.write(reinterpret_cast<char const*>(data.get()), static_cast<std::streamsize>(size));
    if (!out) {
        throw std::runtime_error("failed while writing output file");
    }
}

void SaveLiveViewToDisk(ILiveView const& image, fs::path const& outputPath)
{
    auto data = image.Data();
    auto size = image.DataSizeBytes();
    if (!data || size == 0) {
        throw std::runtime_error("live view buffer is empty");
    }

    if (image.Format() == ImageFormat::JPEG) {
        SaveBufferToDisk(data, size, outputPath);
        return;
    }

    if (image.Format() == ImageFormat::RGB24) {
        auto width = image.DestinationWidth();
        auto height = image.DestinationHeight();
        auto expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 3;
        if (size != expectedSize) {
            throw std::runtime_error("unexpected RGB24 live view payload size");
        }

        std::ofstream out(outputPath, std::ios::binary);
        if (!out) {
            throw std::runtime_error("failed to open output file: " + outputPath.string());
        }

        out << "P6\n" << width << ' ' << height << "\n255\n";
        out.write(reinterpret_cast<char const*>(data.get()), static_cast<std::streamsize>(size));
        if (!out) {
            throw std::runtime_error("failed while writing live view output file");
        }
        return;
    }

    SaveBufferToDisk(data, size, outputPath);
}

std::shared_ptr<IFullImage const> WaitForFullImage(Listener& listener, uint32_t timeoutMs)
{
    while (true) {
        auto notification = listener.WaitForNotification(timeoutMs);
        if (!notification) {
            return nullptr;
        }

        if (notification->Type() == EventType::CameraImageReady) {
            return notification->FullImage();
        }

        if (notification->Type() == EventType::CameraDisconnected) {
            throw std::runtime_error("camera disconnected while waiting for image transfer");
        }

        std::cout << "Ignoring notification: " << notification->Type().name << '\n';
    }
}

void StartLiveViewSubscription(ILiveViewSubscription& liveView)
{
    auto formats = liveView.ImageFormatsSupported();
    auto subscribeFormat = [&](ImageFormat format) -> bool {
        try {
            std::cout << "Subscribing to live view using format " << ImageFormatString(format) << '\n';
            liveView.Subscribe(format);
            return true;
        } catch (SdkException const& ex) {
            std::cout
                << "Live view format " << ImageFormatString(format)
                << " rejected by camera/SDK: " << ex.what()
                << '\n';
            return false;
        }
    };

    for (ImageFormat preferred : {ImageFormat::JPEG, ImageFormat::RGB24, ImageFormat::Auto}) {
        if (subscribeFormat(preferred)) {
            return;
        }
    }

    for (ImageFormat format : formats) {
        if (format != ImageFormat::JPEG &&
            format != ImageFormat::RGB24 &&
            format != ImageFormat::Auto &&
            subscribeFormat(format)) {
            return;
        }
    }

    std::cout << "Subscribing to live view using camera default format\n";
    liveView.Subscribe();
}

std::shared_ptr<ILiveView const> WaitForLiveViewFrame(Listener& listener, uint32_t timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return nullptr;
        }

        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        auto notification = listener.WaitForNotification(static_cast<uint32_t>(remaining));
        if (!notification) {
            return nullptr;
        }

        if (notification->Type() == EventType::CameraLiveViewStarted) {
            std::cout << "Live view started\n";
            continue;
        }

        if (notification->Type() == EventType::CameraNewLiveViewImage) {
            return notification->LiveView();
        }

        if (notification->Type() == EventType::CameraLiveViewStopped) {
            throw std::runtime_error("camera stopped live view before a frame was delivered");
        }

        if (notification->Type() == EventType::CameraDisconnected) {
            throw std::runtime_error("camera disconnected while waiting for live view");
        }

        std::cout << "Ignoring notification: " << notification->Type().name << '\n';
    }
}

std::string ConnectionTypeString(TetherConnection connectionType)
{
    switch (connectionType) {
        case kTetherConnection_USB: return "USB";
        case kTetherConnection_USB_2: return "USB2";
        case kTetherConnection_USB_3: return "USB3";
        case kTetherConnection_TCPIP: return "TCP/IP";
        default: return "Unknown";
    }
}

std::shared_ptr<IStoredImageHandle> FindStoredImage(Camera& camera, std::string const& selector)
{
    auto images = camera.AllImages();
    if (images.empty()) {
        return nullptr;
    }

    if (IsNumeric(selector)) {
        auto wantedId = static_cast<uint32_t>(std::stoul(selector));
        for (auto const& image : images) {
            if (image->Id() == wantedId) {
                return image;
            }
        }
    } else {
        for (auto const& image : images) {
            if (image->Filename() == selector) {
                return image;
            }
        }
    }

    return nullptr;
}

PropertyValue ParsePropertyValue(PropertySpecification const& spec, std::string const& raw)
{
    switch (spec.mValue.mType) {
        case kPropertyTypeBool: {
            PropertyValue value = spec.mValue;
            if (raw == "1" || raw == "true" || raw == "True" || raw == "yes" || raw == "Yes" || raw == "On") {
                value.mInt = 1;
                value.mPresentationString = "Yes";
                return value;
            }
            if (raw == "0" || raw == "false" || raw == "False" || raw == "no" || raw == "No" || raw == "Off") {
                value.mInt = 0;
                value.mPresentationString = "No";
                return value;
            }
            throw std::runtime_error("invalid boolean value: " + raw);
        }
        case kPropertyTypeInt32: {
            PropertyValue value = spec.mValue;
            value.mInt = static_cast<int32_t>(std::stol(raw));
            value.mPresentationString.clear();
            return value;
        }
        case kPropertyTypeInt64: {
            PropertyValue value = spec.mValue;
            value.mInt = static_cast<int64_t>(std::stoll(raw));
            value.mPresentationString.clear();
            return value;
        }
        case kPropertyTypeTime: {
            PropertyValue value = spec.mValue;
            value.mInt = static_cast<int64_t>(std::stoll(raw));
            value.mPresentationString.clear();
            return value;
        }
        case kPropertyTypeUInt32: {
            PropertyValue value = spec.mValue;
            value.mInt = static_cast<uint32_t>(std::stoul(raw));
            value.mPresentationString.clear();
            return value;
        }
        case kPropertyTypeUInt64: {
            PropertyValue value = spec.mValue;
            value.mInt = static_cast<uint64_t>(std::stoull(raw));
            value.mPresentationString.clear();
            return value;
        }
        case kPropertyTypeFloat64: {
            PropertyValue value = spec.mValue;
            value.mDouble = std::stod(raw);
            value.mPresentationString.clear();
            return value;
        }
        case kPropertyTypeString: {
            PropertyValue value = spec.mValue;
            value.mString = raw;
            value.mPresentationString.clear();
            return value;
        }
        case kPropertyTypeEnum:
            for (auto const& candidate : spec.mValueList) {
                if (candidate.StringValue() == raw || candidate.mPresentationString == raw) {
                    return candidate;
                }
            }
            if (IsNumeric(raw)) {
                return PropertyValue::CreateEnum(static_cast<uint32_t>(std::stoul(raw)));
            }
            throw std::runtime_error("enum value not found in property value list: " + raw);
        default:
            throw std::runtime_error("unsupported property type for CLI setter: " + spec.mValue.TypeString());
    }
}

PropertyValue ResolveEnumPropertyValue(PropertySpecification const& spec, std::string const& raw)
{
    if (spec.mValue.mType != kPropertyTypeEnum) {
        throw std::runtime_error("property is not an enum: " + spec.mName);
    }
    return ParsePropertyValue(spec, raw);
}

void PrintSubscriptionSupport(ISubscriptionManager& subscriptions)
{
    std::cout
        << "Subscription support"
        << " | previews=" << (subscriptions.Previews()->IsSupported() ? "yes" : "no")
        << " | liveView=" << (subscriptions.LiveView()->IsSupported() ? "yes" : "no")
        << " | fullImages=" << (subscriptions.FullImages()->IsSupported() ? "yes" : "no")
        << '\n';
}

void PrintKeyProperty(Camera const& camera, uint32_t propertyId, char const* label)
{
    try {
        auto spec = camera.PropertySpec(propertyId);
        std::cout << label << ": " << PropertyValueSummary(spec.mValue) << '\n';
    } catch (SdkException const&) {
        std::cout << label << ": <unavailable>\n";
    }
}

void PrintStatus(Camera& camera)
{
    bool isP5 = IsP5Camera(camera);

    std::cout
        << "Camera handle: " << camera.Id() << '\n'
        << "Connection: " << ConnectionTypeString(camera.ConnectionType()) << '\n';

    PrintSubscriptionSupport(*camera.Subscriptions());
    if (isP5) {
        std::cout << "Live view note: Phase One CameraSDK does not provide LiveView support on P5 cameras.\n";
    }
    PrintKeyProperty(camera, 2, "Model");
    PrintKeyProperty(camera, 3, "Serial");
    PrintKeyProperty(camera, 4, "Firmware");
    PrintKeyProperty(camera, 1201, "Ready For Capture");
    PrintKeyProperty(camera, 1200, "Unsaved Images");
    PrintKeyProperty(camera, 1301, "Local Storage");
    PrintKeyProperty(camera, 1302, "Host Storage");
    PrintKeyProperty(camera, 120, "ISO");
    PrintKeyProperty(camera, 1003, "Aperture");
    PrintKeyProperty(camera, 1002, "Shutter Speed");
    PrintKeyProperty(camera, 1008, "Shutter Mode");
    PrintKeyProperty(camera, 122, "White Balance");
    PrintKeyProperty(camera, 106, "Storage Capacity");
}

void PrintProperties(Camera& camera)
{
    auto propertyIds = camera.AllPropertyIds();
    std::cout << "Discovered " << propertyIds.size() << " camera properties\n";

    for (uint32_t propertyId : propertyIds) {
        try {
            auto spec = camera.PropertySpec(propertyId);
            std::cout
                << "Property " << propertyId
                << " | name=\"" << spec.mName << "\""
                << " | group=\"" << spec.mGroup << "\""
                << " | readOnly=" << (spec.mReadOnly ? "true" : "false")
                << " | enabled=" << (spec.mEnabled ? "true" : "false")
                << " | value=" << PropertyValueSummary(spec.mValue)
                << '\n';
        } catch (SdkException const& ex) {
            std::cerr << "Property " << propertyId << " could not be read: " << ex.what() << '\n';
        }
    }
}

void PrintPropertyInfo(Camera& camera, std::string const& propertyIdText)
{
    if (!IsNumeric(propertyIdText)) {
        throw std::runtime_error("property-info requires a numeric property id");
    }

    auto propertyId = static_cast<uint32_t>(std::stoul(propertyIdText));
    auto spec = camera.PropertySpec(propertyId);

    std::cout
        << "Property " << propertyId << '\n'
        << "Name: " << spec.mName << '\n'
        << "Group: " << spec.mGroup << '\n'
        << "Type: " << spec.mValue.TypeString() << '\n'
        << "Current Value: " << PropertyValueSummary(spec.mValue) << '\n'
        << "Read Only: " << (spec.mReadOnly ? "true" : "false") << '\n'
        << "Enabled: " << (spec.mEnabled ? "true" : "false") << '\n'
        << "Select Only: " << (spec.mSelectOnly ? "true" : "false") << '\n';

    if (spec.mRange.mHas) {
        std::cout << "Range: " << spec.mRange.ToString() << '\n';
    } else {
        std::cout << "Range: <none>\n";
    }

    if (!spec.mValueList.empty()) {
        std::cout << "Allowed Values:\n";
        for (auto const& value : spec.mValueList) {
            std::cout << "  - " << PropertyValueSummary(value) << '\n';
        }
    } else {
        std::cout << "Allowed Values: <none>\n";
    }
}

void SetPropertyById(Camera& camera, uint32_t propertyId, std::string const& rawValue)
{
    auto spec = camera.PropertySpec(propertyId);
    if (spec.mReadOnly) {
        throw std::runtime_error("property is read-only: " + spec.mName);
    }
    if (!spec.mEnabled) {
        throw std::runtime_error("property is disabled: " + spec.mName);
    }

    auto value = ParsePropertyValue(spec, rawValue);
    camera.SetProperty(propertyId, value);

    auto updated = camera.PropertySpec(propertyId);
    std::cout
        << "Updated property " << propertyId
        << " | name=\"" << updated.mName << "\""
        << " | value=" << PropertyValueSummary(updated.mValue)
        << '\n';
}

void SetPropertyValue(Camera& camera, uint32_t propertyId, PropertyValue const& value)
{
    auto spec = camera.PropertySpec(propertyId);
    if (spec.mReadOnly) {
        throw std::runtime_error("property is read-only: " + spec.mName);
    }
    if (!spec.mEnabled) {
        throw std::runtime_error("property is disabled: " + spec.mName);
    }

    camera.SetProperty(propertyId, value);

    auto updated = camera.PropertySpec(propertyId);
    std::cout
        << "Updated property " << propertyId
        << " | name=\"" << updated.mName << "\""
        << " | value=" << PropertyValueSummary(updated.mValue)
        << '\n';
}

void SetCameraIpAddress(
    Camera& camera,
    std::string const& newIp,
    std::string const& newNetmask,
    std::string const& newGateway)
{
    auto applyNowValue = ResolveEnumPropertyValue(camera.PropertySpec(kPropertyNetworkStatusAction), "Apply Now");

    std::cout
        << "Updating network settings"
        << " | ip=" << newIp
        << " | netmask=" << (newNetmask.empty() ? "<unchanged>" : newNetmask)
        << " | gateway=" << (newGateway.empty() ? "<unchanged>" : newGateway)
        << '\n';

    SetPropertyById(camera, kPropertyStaticSetup, "Yes");
    SetPropertyById(camera, kPropertyDhcpEnabled, "No");
    SetPropertyById(camera, kPropertyIpAddress, newIp);

    if (!newNetmask.empty()) {
        SetPropertyById(camera, kPropertyNetmask, newNetmask);
    }
    if (!newGateway.empty()) {
        SetPropertyById(camera, kPropertyGateway, newGateway);
    }
    SetPropertyValue(camera, kPropertyNetworkStatusAction, applyNowValue);

    std::cout << "Network change requested. Reconnect using the new IP address if the session drops.\n";
}

void PrintStoredImages(Camera& camera)
{
    auto images = camera.AllImages();
    std::cout << "Stored images: " << images.size() << '\n';

    for (auto const& image : images) {
        std::cout
            << "Image " << image->Id()
            << " | file=\"" << image->Filename() << "\""
            << " | date=" << image->Date()
            << " | devices=" << image->DeviceString()
            << '\n';
    }
}

void CaptureToHost(Camera& camera, std::string const& requestedPath)
{
    auto fullImages = camera.Subscriptions()->FullImages();
    if (!fullImages->IsSupported()) {
        throw std::runtime_error("camera does not support host full-image transfers");
    }

    Listener listener;
    listener.EnableNotification(camera, EventType::CameraImageReady);
    listener.EnableNotification(camera, EventType::CameraDisconnected);
    fullImages->Subscribe();

    std::cout << "Triggering capture\n";
    camera.TriggerCapture();

    auto image = WaitForFullImage(listener, 60000);
    if (!image) {
        fullImages->Unsubscribe();
        throw std::runtime_error("timed out waiting for CameraImageReady");
    }

    fs::path outputPath = CaptureOutputPath(*image, requestedPath);
    SaveBufferToDisk(image->Data(), image->DataSizeBytes(), outputPath);

    std::cout
        << "Captured file saved to " << outputPath
        << " | bytes=" << image->DataSizeBytes()
        << " | camera name=\"" << image->FileName() << "\""
        << '\n';

    fullImages->Unsubscribe();
    listener.WakeUpWaitingThread();
}

void CaptureLiveViewFrame(Camera& camera, std::string const& requestedPath)
{
    if (IsP5Camera(camera)) {
        throw std::runtime_error("live view is not supported on P5 cameras by Phase One CameraSDK 3.1.8");
    }

    auto liveView = camera.Subscriptions()->LiveView();
    if (!liveView->IsSupported()) {
        throw std::runtime_error("camera does not support live view");
    }

    Listener listener;
    listener.EnableNotification(camera, EventType::CameraLiveViewStarted);
    listener.EnableNotification(camera, EventType::CameraNewLiveViewImage);
    listener.EnableNotification(camera, EventType::CameraLiveViewStopped);
    listener.EnableNotification(camera, EventType::CameraDisconnected);

    try {
        StartLiveViewSubscription(*liveView);
        auto image = WaitForLiveViewFrame(listener, 30000);
        if (!image) {
            throw std::runtime_error("timed out waiting for live view frame");
        }

        fs::path outputPath = LiveViewOutputPath(*image, requestedPath);
        SaveLiveViewToDisk(*image, outputPath);

        std::cout
            << "Live view frame saved to " << outputPath
            << " | bytes=" << image->DataSizeBytes()
            << " | format=" << ImageFormatString(image->Format())
            << " | size=" << image->DestinationWidth() << 'x' << image->DestinationHeight()
            << '\n';

        liveView->Unsubscribe();
        listener.WakeUpWaitingThread();
    } catch (...) {
        if (liveView->IsSubscribed()) {
            liveView->Unsubscribe();
        }
        listener.WakeUpWaitingThread();
        throw;
    }
}

void DownloadStoredImage(Camera& camera, std::string const& selector, std::string const& requestedPath)
{
    auto imageHandle = FindStoredImage(camera, selector);
    if (!imageHandle) {
        throw std::runtime_error("stored image not found: " + selector);
    }

    auto fullImages = camera.Subscriptions()->FullImages();
    if (!fullImages->IsSupported()) {
        throw std::runtime_error("camera does not support host full-image transfers");
    }

    Listener listener;
    listener.EnableNotification(camera, EventType::CameraImageReady);
    listener.EnableNotification(camera, EventType::CameraDisconnected);
    fullImages->Subscribe();

    if (!imageHandle->RequestImageAsync()) {
        fullImages->Unsubscribe();
        throw std::runtime_error("camera rejected stored image transfer request");
    }

    auto image = WaitForFullImage(listener, 60000);
    if (!image) {
        fullImages->Unsubscribe();
        throw std::runtime_error("timed out waiting for stored image transfer");
    }

    fs::path outputPath = requestedPath.empty() ? fs::path(imageHandle->Filename()) : fs::path(requestedPath);
    SaveBufferToDisk(image->Data(), image->DataSizeBytes(), outputPath);

    std::cout
        << "Stored image saved to " << outputPath
        << " | bytes=" << image->DataSizeBytes()
        << " | imageId=" << imageHandle->Id()
        << '\n';

    fullImages->Unsubscribe();
    listener.WakeUpWaitingThread();
}

class LoggingGuard {
public:
    LoggingGuard()
    {
        SetSdkLoggingHandler([](char const* message, LogMessageLevel level) {
            char const* label = "INFO";
            switch (level) {
                case LogMessageLevel::ERROR: label = "ERROR"; break;
                case LogMessageLevel::WARNING: label = "WARN"; break;
                case LogMessageLevel::INFO: label = "INFO"; break;
                case LogMessageLevel::VERBOSE: label = "VERBOSE"; break;
            }
            std::cerr << "[CameraSDK][" << label << "] " << message << '\n';
        });
    }

    ~LoggingGuard()
    {
        ClearSdkLoggingHandler();
    }
};

}  // namespace

int main(int argc, char** argv)
{
    try {
        Options options = ParseArgs(argc, argv);
        if (options.help) {
            PrintUsage(argv[0]);
            return 0;
        }
        LoggingGuard loggingGuard;

        auto sdkVersion = GetSdkVersion();
        std::cout
            << "CameraSDK version "
            << sdkVersion.major << '.'
            << sdkVersion.minor << '.'
            << sdkVersion.build
            << " | build date: " << GetSdkBuildDate()
            << '\n';

        Camera camera = Camera::OpenIpCamera(options.ip, options.password, 5);
        std::cout << "Opened camera at " << options.ip << '\n';

        if (options.command == "status") {
            PrintStatus(camera);
        } else if (options.command == "properties") {
            PrintProperties(camera);
        } else if (options.command == "capture") {
            CaptureToHost(camera, options.argument);
        } else if (options.command == "live-view") {
            CaptureLiveViewFrame(camera, options.argument);
        } else if (options.command == "images") {
            PrintStoredImages(camera);
        } else if (options.command == "download-image") {
            std::string requestedPath;
            if (argc >= 5) {
                std::string maybePath = argv[4];
                if (maybePath.find('/') != std::string::npos || maybePath.find('.') != std::string::npos) {
                    requestedPath = maybePath;
                }
            }
            DownloadStoredImage(camera, options.argument, requestedPath);
        } else if (options.command == "property-info") {
            PrintPropertyInfo(camera, options.argument);
        } else if (options.command == "set-property") {
            if (!IsNumeric(options.argument)) {
                throw std::runtime_error("set-property requires a numeric property id");
            }
            SetPropertyById(camera, static_cast<uint32_t>(std::stoul(options.argument)), options.argument2);
        } else if (options.command == "set-ip") {
            SetCameraIpAddress(camera, options.argument, options.argument2, options.argument3);
        } else if (options.command == "set-iso") {
            SetPropertyById(camera, 120, options.argument);
        } else if (options.command == "set-aperture") {
            SetPropertyById(camera, 1003, options.argument);
        } else if (options.command == "set-shutter-us") {
            SetPropertyById(camera, 1012, options.argument);
        } else if (options.command == "set-shutter") {
            SetPropertyById(camera, 1002, options.argument);
        } else if (options.command == "set-exposure-program") {
            SetPropertyById(camera, 1000, options.argument);
        } else if (options.command == "set-host-storage") {
            SetPropertyById(camera, 1302, options.argument);
        } else if (options.command == "set-local-storage") {
            SetPropertyById(camera, 1301, options.argument);
        }

        camera.Close();
        return 0;
    } catch (SdkException const& ex) {
        std::cerr << "SDK error: " << ex.what() << " (code=" << ex.mErrorCode << ")\n";
    } catch (std::exception const& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
    }

    return 1;
}
