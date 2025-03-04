#include <chrono>
#include <iostream>
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <vector>
#include <cstdlib>

#define BUFFER_LENGTH 64
HANDLE hidDeviceObject = INVALID_HANDLE_VALUE;

bool loadHidLibrary(HMODULE& hHidLib, GUID& classGuid);
bool getUSBDevices(HDEVINFO& deviceInfoSet, GUID& classGuid, PSP_DEVICE_INTERFACE_DETAIL_DATA& deviceInterfaceDetailData);
bool openUSBDevice(HANDLE& hidDeviceObject, PSP_DEVICE_INTERFACE_DETAIL_DATA& deviceInterfaceDetailData, HDEVINFO& deviceInfoSet);

bool connectUSBDevice(HMODULE& hHidLib, GUID& classGuid, HDEVINFO& deviceInfoSet, PSP_DEVICE_INTERFACE_DETAIL_DATA& deviceInterfaceDetailData, HANDLE& hidDeviceObject) {
    if(!loadHidLibrary(hHidLib, classGuid)) {
        std::cerr << "Failed to load HID library" << std::endl;
        FreeLibrary(hHidLib);
        return false;
    }

    if(!getUSBDevices(deviceInfoSet, classGuid, deviceInterfaceDetailData)) {
        std::cerr << "Error getting device info" << std::endl;
        FreeLibrary(hHidLib);
        SetupDiDestroyDeviceInfoList(deviceInfoSet);

        return false;
    }

    if(!openUSBDevice(hidDeviceObject, deviceInterfaceDetailData, deviceInfoSet)) {
        std::cerr << "Error opening USB device" << std::endl;
        FreeLibrary(hHidLib);
        SetupDiDestroyDeviceInfoList(deviceInfoSet);

        return false;
    }

    return true;
}

bool loadHidLibrary(HMODULE& hHidLib, GUID& classGuid) {
    void(__stdcall *HidD_GetHidGuid)(OUT LPGUID HidGuid);

    hHidLib = LoadLibrary("C:\\Windows\\system32\\HID.DLL");
    if (hHidLib == NULL) {
        std::cout << "Error loading hid.lib" << std::endl;
        return -1;
    }

    (FARPROC&) HidD_GetHidGuid=GetProcAddress(hHidLib, "HidD_GetHidGuid");

    if (!HidD_GetHidGuid) {
        std::cout << "Error loading hid.lib" << std::endl;
        FreeLibrary(hHidLib);
        return false;
    }

    HidD_GetHidGuid(&classGuid);

    return true;
}

bool getUSBDevices(HDEVINFO& deviceInfoSet, GUID& classGuid, PSP_DEVICE_INTERFACE_DETAIL_DATA& deviceInterfaceDetailData) {
    DWORD memberIndex = 0;
    unsigned long requiredSize;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    unsigned long deviceInterfaceDetailDataSize = 0;
    HIDD_ATTRIBUTES hiddAttributes;


    deviceInfoSet = SetupDiGetClassDevs(&classGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }

    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    hiddAttributes.Size = sizeof(HIDD_ATTRIBUTES);

    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &classGuid, memberIndex, &deviceInterfaceData)) {
        ++memberIndex;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &deviceInterfaceDetailDataSize, NULL);
        requiredSize = deviceInterfaceDetailDataSize;
        deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) new u_long[requiredSize];
        deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);


        if (!SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, deviceInterfaceDetailData, deviceInterfaceDetailDataSize, &requiredSize, NULL)) {
            delete[] deviceInterfaceDetailData;
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            return false;
        }

        std::string devicePath(deviceInterfaceDetailData->DevicePath);

        if(devicePath.find("vid_054c") != std::string::npos) {
            return true;
        }

    }

    return false;
}

bool openUSBDevice(HANDLE& hidDeviceObject, PSP_DEVICE_INTERFACE_DETAIL_DATA& deviceInterfaceDetailData, HDEVINFO& deviceInfoSet) {
    CloseHandle(hidDeviceObject);
    if (!deviceInterfaceDetailData || deviceInterfaceDetailData->DevicePath[0] == L'\0') {
        std::cout << "Invalid device interface data or empty DevicePath." << std::endl;
        return false;
    }

    hidDeviceObject = CreateFile(
        deviceInterfaceDetailData->DevicePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hidDeviceObject == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::cout << "Error opening device: " << error << std::endl;
        return false;
    }

    std::cout << "Device successfully opened: " << deviceInterfaceDetailData->DevicePath << std::endl;

    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return true;
}

int readUSBDevice(HANDLE& hidDeviceObject) {
    BYTE inputReportBuffer[BUFFER_LENGTH];
    DWORD bytesRead = 0;

        memset(&inputReportBuffer, 0x00, sizeof(inputReportBuffer));

        if (!ReadFile(hidDeviceObject, inputReportBuffer, sizeof(inputReportBuffer), &bytesRead, NULL)) {
            DWORD error = GetLastError();
            std::cout << "ReadFile failed with error: " << error << std::endl;
            return -1;
        }

        return inputReportBuffer[8];
}

// height = 3*width for visual purposes

const int width = 60;
const int height = 20;

int delayInMs = 300;
int tmp;
int buttonCounter = 0;
int appleCounter = 0;
int gameStatus;

std::vector<std::pair<int,int>> bodyPositions;
std::pair<int,int> applePosition;

char** field = new char* [width];

std::string snakeColor = "\033[32m";


enum Direction {
    RIGHT = 2,
    DOWN = 0,
    LEFT = 6,
    UP = 4,
    NEUTRAL = 8
};

Direction direction = RIGHT;

bool appleEaten = true;
bool delayModified = false;

void initGame() {
    bodyPositions.push_back(std::pair(width/3/2+1, height/2));
    bodyPositions.push_back(std::pair(width/3/2, height/2));
    bodyPositions.push_back(std::pair(width/3/2-1, height/2));

    for (int i = 0; i < width; i++) {
        field[i] = new char[height];
    }

    for(int i = 0; i < width; i++) {
        field[i][0] = '-';
    }

    for(int i = 1; i < height; i++) {
        for(int j = 0; j < width; j++) {
            if(j == 0 || j == width - 1) {
                field[j][i] = '|';
            } else {
                field[j][i] = ' ';
            }
        }
    }

    for(int i = 0; i < width; i++) {
        field[i][height-1] = char(151);
    }
}

void generateApple() {
    do {
        applePosition.first = (rand() % (width/3 - 2)) + 1;  // Keep it within limits
        applePosition.second = (rand() % (height - 2)) + 1;
    } while ([&]() -> bool {
        for (auto& pos : bodyPositions) {
            if (pos.first == applePosition.first && pos.second == applePosition.second) {
                return true;
            }
        }
        return false;
    }());

    // Fix apple placement by scaling width
    field[applePosition.first * 3][applePosition.second] = '@';
    appleEaten = false;
}


int updateSnakePosition() {
    for(auto & pos : bodyPositions) {
        field[pos.first*3][pos.second] = ' ';
    }

    for(int i = bodyPositions.size() - 1; i > 0; --i) {
        bodyPositions[i].first = bodyPositions[i-1].first;
        bodyPositions[i].second = bodyPositions[i-1].second;
    }

    switch (direction) {
        case RIGHT:
            ++bodyPositions[0].first;
        break;

        case UP:
            ++bodyPositions[0].second;
        break;

        case LEFT:
            --bodyPositions[0].first;
        break;

        case DOWN:
            --bodyPositions[0].second;
        break;
    }

    if(bodyPositions.size() >= ((width/3)-1)*(height-2)) {
        return 2;
    }

    if(bodyPositions[0].first == 0 || bodyPositions[0].first == width / 3 || bodyPositions[0].second == 0 || bodyPositions[0].second == height-1) {
        return 0;
    }

    for(int i = bodyPositions.size() - 1; i > 0; --i) {
        if(bodyPositions[i].first == bodyPositions[0].first && bodyPositions[i].second == bodyPositions[0].second) {
            return 0;
        }
    }

    for(int i = 0; i < bodyPositions.size(); ++i) {
        if(i == 0)
            field[bodyPositions[i].first*3][bodyPositions[i].second] = '\x01';
        else
            field[bodyPositions[i].first*3][bodyPositions[i].second] = '\x02';
    }

    if(bodyPositions[0].first == applePosition.first && bodyPositions[0].second == applePosition.second) {
        appleEaten = true;
        bodyPositions.push_back(bodyPositions[0]);
    }

    return 1;
}

void renderMap() {
    std::cout << "\033[?25l"; // Hide cursor
    std::string buffer;

    for (int i = 0; i < height; ++i) {
        for (int j = 0; j < width; ++j) {
            if (field[j][i] == '\x01' || field[j][i] == '\x02') {
                buffer += snakeColor;
                buffer += (field[j][i] == '\x01' ? char(214) : char(212));
                buffer += "\033[0m";
            } else if (field[j][i] == '@') {
                buffer += "\033[38;5;214m";
                buffer += '@';
                buffer += "\033[0m";
            } else {
                buffer += field[j][i];
            }
        }
        buffer += '\n';
    }

    std::cout << "\033[H" << buffer;
}


void startGame() {
    system("cls");
    initGame();
    renderMap();

    while(true) {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = start + std::chrono::milliseconds(delayInMs);

        delayModified = false;
        buttonCounter = 0;

        while(std::chrono::high_resolution_clock::now() < end) {
            tmp = readUSBDevice(hidDeviceObject);
            if(tmp != 8 && static_cast<Direction>(tmp-4) != direction && static_cast<Direction>(tmp+4) != direction && (tmp == 0 || tmp == 2 || tmp == 4 || tmp == 6)) {
                direction = static_cast<Direction>(tmp);
            }

            if(tmp == 40) {
                if(buttonCounter != 10) {
                    ++buttonCounter;
                    continue;
                }
                if(!delayModified) {
                    delayInMs = (delayInMs == 300 ? 100 : 300);
                    snakeColor = (snakeColor == "\033[32m" ? "\033[31m" :  "\033[32m");
                    delayModified = true;
                }
            }
        }

        gameStatus = updateSnakePosition();

        if(gameStatus == 2) {
            std::cout << "You won" << std::endl;
            break;
        } else if(gameStatus == 0) {
            std::cout << "You died" << std::endl;
            break;
        }

        if(appleEaten) {
            generateApple();
        }

        renderMap();
    }

}

int main() {
    SetConsoleCP(1252);
    SetConsoleOutputCP(1252);

    std::srand(std::time(0));

    GUID classGuid;
    HMODULE hHidLib;
    HDEVINFO deviceInfoSet;
    PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData;

    if(!connectUSBDevice(hHidLib, classGuid, deviceInfoSet, deviceInterfaceDetailData, hidDeviceObject)) {
        CloseHandle(hidDeviceObject);
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        FreeLibrary(hHidLib);

        return -1;
    }

    startGame();

    CloseHandle(hidDeviceObject);
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    FreeLibrary(hHidLib);

    system("pause");

    return 0;
}
