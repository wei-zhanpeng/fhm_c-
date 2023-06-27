#include <array>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
//setupapi.h
#include <stdio.h>
#include <string>
#include <Windows.h>
#include <tchar.h>
#include <SetupAPI.h>
#include <INITGUID.H>
//设备的路径，使用xdma_public.h文件中的宏定义可以访问对应的数据接口
char device_base_path[MAX_PATH + 1] = "";

/* helper struct to remember the Xdma device names 
typedef struct {
	TCHAR base_path[MAX_PATH + 1]; //path to first found Xdma device 
	TCHAR c2h0_path[MAX_PATH + 1];	//card to host DMA 0 
	TCHAR h2c0_path[MAX_PATH + 1];	// host to card DMA 0
	PBYTE buffer; // pointer to the allocated buffer 
	DWORD buffer_size; // size of the buffer in bytes 
	HANDLE c2h0;
	HANDLE h2c0;
} xdma_device;*/

std::string get_windows_error_msg() {

    char msg_buffer[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg_buffer, 256, NULL);
    return{ msg_buffer, 256 };
}

//-----------------------windows device handle----------------------------//
struct device_file {
    HANDLE h;
    device_file(const std::string& path, DWORD accessFlags);
    ~device_file();

    void seek(long device_offset);
    void reoffset(long device_offset);
    size_t write(void* buffer, size_t size);
    size_t read(void* buffer, size_t size);

    template <typename T>
    T read_intr(long address) {
        if (INVALID_SET_FILE_POINTER == SetFilePointer(h, address, NULL, FILE_BEGIN)) {
            throw std::runtime_error("SetFilePointer failed: " + get_windows_error_msg());
        }
        T buffer;
        unsigned long num_bytes_read;
        if (!ReadFile(h, &buffer, sizeof(T), &num_bytes_read, NULL)) {
            throw std::runtime_error("Failed to read from stream! " + get_windows_error_msg());
        } else if (num_bytes_read != sizeof(T)) {
            throw std::runtime_error("Failed to read all bytes!");
        }
        return buffer;
    }
};

device_file::device_file(const std::string& path, DWORD accessFlags) {
    h = CreateFile(path.c_str(), accessFlags, 0, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
}

device_file::~device_file() {
    CloseHandle(h);
}

void device_file::seek(long device_offset) {
    if (INVALID_SET_FILE_POINTER == SetFilePointer(h, device_offset, NULL, FILE_CURRENT)) {
        throw std::runtime_error("SetFilePointer failed: " + std::to_string(GetLastError()));
    }
}
void device_file::reoffset(long device_offset) {
    if (INVALID_SET_FILE_POINTER == SetFilePointer(h, device_offset, NULL, FILE_BEGIN)) {
        throw std::runtime_error("SetFilePointer failed: " + std::to_string(GetLastError()));
    }
}
size_t device_file::write(void* buffer, size_t size) {
    unsigned long num_bytes_written;
    if (!WriteFile(h, buffer, (DWORD)size, &num_bytes_written, NULL)) {
        throw std::runtime_error("Failed to write to device! " + std::to_string(GetLastError()));
    }

    return num_bytes_written;
}

size_t device_file::read(void* buffer, size_t size) {
    unsigned long num_bytes_read;
    if (!ReadFile(h, buffer, (DWORD)size, &num_bytes_read, NULL)) {
        throw std::runtime_error("Failed to read from stream! " + std::to_string(GetLastError()));
    }
    return num_bytes_read;
}

//------------------------xdma device-----------------------//
/*class xdma_device {
public:
    xdma_device(const string& device_path);
    ~xdma_device();
    void print_details();
private:
    HANDLE control = NULL;
    uint32_t read_register(long addr);
    void read_block(long addr, size_t size, void* buffer);
    void print_block(long offset);
    void print_channel_module(long offset);
    void print_irq_module(long module_base);
    void print_config_module(long offset);
    void print_sgdma_module(long offset);
    void print_sgdma_common_module(long offset);

    uint32_t regs[0xD4 / sizeof(uint32_t)];

    inline uint32_t reg_at (size_t addr) {
        return (regs[addr / sizeof(uint32_t)]);
    };
};
xdma_device::xdma_device(const string& device_path) {
    using namespace std::string_literals;
    string minor_dev_path = device_path + "\\control"s;

    control = CreateFile(minor_dev_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (control == INVALID_HANDLE_VALUE) {
        throw runtime_error("CreateFile failed: " + std::to_string(GetLastError()));
    }
}

xdma_device::~xdma_device() {
    CloseHandle(control);
}

void xdma_device::print_channel_module(long module_base) {
    cout << std::boolalpha << std::hex;

    for (unsigned i = 0; i < 4; ++i) {

        const long base = module_base + i * 0x100;
        read_block(base, 0xD4, regs);

        if (reg_at(0x0) & 0x1cf00000) {
            cout << "Channel ID:\t\t" << get_bits(reg_at(0x0), 8, 4) << '\n';
            cout << " Version:\t\t" << version_to_string(reg_at(0x0)) << '\n';
            cout << " Streaming:\t\t" << is_bit_set(reg_at(0x0), 15) << '\n';
            cout << " Running:\t\t" << is_bit_set(reg_at(0x4), 0) << '\n';
            cout << " IE descr stop:\t\t" << is_bit_set(reg_at(0x4), 1) << '\n';
            cout << " IE descr complete:\t" << is_bit_set(reg_at(0x4), 2) << '\n';
            cout << " IE align mismatch:\t" << is_bit_set(reg_at(0x4), 3) << '\n';
            cout << " IE magic stopped:\t" << is_bit_set(reg_at(0x4), 4) << '\n';
            cout << " IE invalid length:\t" << is_bit_set(reg_at(0x4), 5) << '\n';
            cout << " IE idle stopped:\t" << is_bit_set(reg_at(0x4), 6) << '\n';
            cout << " IE read error:\t\t" << get_bits(reg_at(0x4), 9, 5) << '\n';
            cout << " IE write error:\t" << get_bits(reg_at(0x4), 14, 5) << '\n';
            cout << " IE descr error:\t" << get_bits(reg_at(0x4), 19, 5) << '\n';
            cout << " Non incremental mode:\t" << is_bit_set(reg_at(0x4), 25) << '\n';
            cout << " Pollmode wb:\t\t" << is_bit_set(reg_at(0x4), 26) << '\n';
            cout << " AXI-ST wb disabled:\t" << is_bit_set(reg_at(0x4), 27) << '\n';
            cout << " Busy:\t\t\t" << is_bit_set(reg_at(0x40), 0) << '\n';
            cout << " Descriptor stopped:\t" << is_bit_set(reg_at(0x40), 1) << '\n';
            cout << " Descriptor completed:\t" << is_bit_set(reg_at(0x40), 2) << '\n';
            cout << " Alignment mismatch:\t" << is_bit_set(reg_at(0x40), 3) << '\n';
            cout << " Magic stopped:\t\t" << is_bit_set(reg_at(0x40), 4) << '\n';
            cout << " Invalid length:\t" << is_bit_set(reg_at(0x40), 5) << '\n';
            cout << " Idle stopped:\t\t" << is_bit_set(reg_at(0x40), 6) << '\n';
            cout << " Read error:\t\t0x" << get_bits(reg_at(0x40), 9, 5) << '\n';
            cout << " Write error:\t\t0x" << get_bits(reg_at(0x40), 14, 5) << '\n';
            cout << " Descriptor error:\t0x" << get_bits(reg_at(0x40), 19, 5) << '\n';
            cout << " Completed Descriptors:\t" << std::dec << reg_at(0x48) << '\n';
            cout << " Addr Alignment:\t" << get_bits(reg_at(0x4C), 16, 7) << " bytes\n";
            cout << " Len Granularity:\t" << get_bits(reg_at(0x4C), 8, 7) << " bytes\n";
            cout << " Addr bits:\t\t" << get_bits(reg_at(0x4C), 0, 7) << " bits\n";
            cout << " Poll wb addr lo:\t0x" << std::hex << reg_at(0x88) << '\n';
            cout << " Poll wb addr hi:\t0x" << reg_at(0x8C) << '\n';
            cout << " IM Descr stopped:\t" << is_bit_set(reg_at(0x90), 1) << '\n';
            cout << " IM Descr completed:\t" << is_bit_set(reg_at(0x90), 2) << '\n';
            cout << " IM Alignment mismatch:\t" << is_bit_set(reg_at(0x90), 3) << '\n';
            cout << " IM Magic stopped:\t" << is_bit_set(reg_at(0x90), 4) << '\n';
            cout << " IM Invalid length:\t" << is_bit_set(reg_at(0x90), 5) << '\n';
            cout << " IM Idle stopped:\t" << is_bit_set(reg_at(0x90), 6) << '\n';
            cout << " IM Read error:\t\t0x" << get_bits(reg_at(0x90), 9, 5) << '\n';
            cout << " IM Write error:\t0x" << get_bits(reg_at(0x90), 14, 5) << '\n';
            cout << " IM Descriptor error:\t0x" << get_bits(reg_at(0x90), 19, 5) << '\n';
            cout << '\n';
        }
    }
}

void xdma_device::print_irq_module(long base) {

    read_block(base, 0xA8, regs);

    cout << std::boolalpha;
    cout << " Version:\t\t" << version_to_string(reg_at(0x0)) << '\n';
    cout << " User IRQ en mask:\t0x" << std::hex << reg_at(0x4) << '\n';
    cout << " Chan IRQ en mask:\t0x" << reg_at(0x10) << '\n';
    cout << " User IRQ:\t\t0x" << reg_at(0x40) << '\n';
    cout << " Chan IRQ:\t\t0x" << reg_at(0x44) << '\n';
    cout << " User IRQ pending:\t0x" << reg_at(0x48) << '\n';
    cout << " Chan IRQ pending:\t0x" << reg_at(0x4c) << '\n';

    const unsigned regSize = 4; // number of vector indexes stored in each 32bit register
    cout << std::dec;
    for (unsigned j = 0; j < 4; ++j) { // 4 registers each containing 4 vector indexes
        uint32_t reg_val = reg_at(0x80 + (regSize * j));
        for (unsigned i = 0; i < regSize; ++i) {
            cout << " User IRQ Vector " << (i + regSize * j) << ":\t" << get_bits(reg_val, i * 8, 5) << '\n';
        }
    }

    for (unsigned j = 0; j < 2; ++j) { // 2 registers each containing 4 vector indexes
        uint32_t reg_val = reg_at(0xA0 + (regSize * j));
        for (unsigned i = 0; i < regSize; ++i) {
            cout << " Chan IRQ Vector " << (i + regSize * j) << ":\t" << get_bits(reg_val, i * 8, 5) << '\n';
        }
    }
}

void xdma_device::print_config_module(long base) {

    read_block(base, 0x64, regs);

    cout << std::dec << std::boolalpha;
    cout << " Version:\t\t" << version_to_string(reg_at(0x0)) << '\n';
    cout << " PCIe bus:\t\t" << get_bits(reg_at(0x4), 8, 4) << '\n';
    cout << " PCIe device:\t\t" << get_bits(reg_at(0x4), 3, 5) << '\n';
    cout << " PCIe function:\t\t" << get_bits(reg_at(0x4), 0, 3) << '\n';
    cout << " PCIE MPS:\t\t" << (1 << (7 + reg_at(0x8))) << " bytes\n";
    cout << " PCIE MRRS:\t\t" << (1 << (7 + reg_at(0xC))) << " bytes\n";
    cout << " System ID:\t\t0x" << std::hex << reg_at(0x10) << '\n';
    cout << " MSI support:\t\t" << is_bit_set(reg_at(0x14), 0) << '\n';
    cout << " MSI-X support:\t\t" << is_bit_set(reg_at(0x14), 1) << '\n';
    cout << " PCIE Data Width:\t" << std::dec << (1 << (6 + reg_at(0x18))) << " bits\n";
    cout << " PCIE Control:\t\t0x" << std::hex << reg_at(0x1C) << "\n";
    cout << " User PRG MPS:\t\t" << std::dec << (1 << (7 + get_bits(reg_at(0x40), 0, 3))) << " bytes\n";
    cout << " User EFF MPS:\t\t" << std::dec << (1 << (7 + get_bits(reg_at(0x40), 4, 3))) << " bytes\n";
    cout << " User PRG MRRS:\t\t" << std::dec << (1 << (7 + get_bits(reg_at(0x44), 0, 3))) << " bytes\n";
    cout << " User EFF MRRS:\t\t" << std::dec << (1 << (7 + get_bits(reg_at(0x44), 4, 3))) << " bytes\n";
    cout << " Write Flush Timeout:\t0x" << std::hex << reg_at(0x60) << "\n";
}

void xdma_device::print_sgdma_module(long module_base) {
    cout << std::boolalpha << std::hex;
    for (unsigned i = 0; i < 4; ++i) {
        long base = module_base + i * 0x100;
        read_block(base, 0x90, regs);

        if (reg_at(0x0) & 0x1cf00000) {
            cout << "Channel ID:\t\t" << get_bits(reg_at(0x0), 8, 4) << '\n';
            cout << " Version:\t\t" << version_to_string(reg_at(0x0)) << '\n';
            cout << " Streaming:\t\t" << is_bit_set(reg_at(0x0), 15) << '\n';
            cout << " Descr addr lo:\t\t0x" << std::hex << reg_at(0x80) << '\n';
            cout << " Descr addr hi:\t\t0x" << reg_at(0x84) << '\n';
            cout << " Adj Descriptors:\t" << std::dec << reg_at(0x88) << '\n';
            cout << " Descr credits:\t\t" << reg_at(0x8C) << '\n';
        }
    }
}

void xdma_device::print_sgdma_common_module(long base) {

    read_block(base, 0x24, regs);

    cout << std::hex << std::boolalpha;
    cout << " Version:\t\t" << version_to_string(reg_at(0x0)) << '\n';
    cout << " Halt H2C descr fetch:\t0x" << get_bits(reg_at(0x10), 0, 4) << '\n';
    cout << " Halt C2H descr fetch:\t0x" << get_bits(reg_at(0x10), 16, 4) << '\n';
    cout << " H2C descr credit:\t0x" << get_bits(reg_at(0x20), 0, 4) << '\n';
    cout << " C2H descr credit:\t0x" << get_bits(reg_at(0x20), 16, 4) << '\n';
}

void xdma_device::print_block(long base) {
    uint32_t id = read_register(base);
    auto module_type = id_to_module(id);
    cout << to_string(module_type) << " Module\n";

    switch (module_type) {
    case h2c_block:
    case c2h_block:
        print_channel_module(base);
        break;
    case irq_block:
        print_irq_module(base);
        break;
    case config_block:
        print_config_module(base);
        break;
    case h2c_sgdma_block:
    case c2h_sgdma_block:
        print_sgdma_module(base);
        break;
    case sgdma_common_block:
        print_sgdma_common_module(base);
        break;
    default:
        break;
    }

    cout << '\n';
}

void xdma_device::print_details() {
    cout << std::hex;

    for (long i = 0; i < 7; ++i) {
        print_block(i * 0x1000);
    }
}

uint32_t xdma_device::read_register(long addr) {
    int value = -1;
    size_t num_bytes_read;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(control, addr, NULL, FILE_BEGIN)) {
        throw runtime_error("SetFilePointer failed: " + std::to_string(GetLastError()));
    }
    if (!ReadFile(control, (LPVOID)&value, 4, (LPDWORD)&num_bytes_read, NULL)) {
        throw runtime_error("ReadFile failed:" + std::to_string(GetLastError()));
    }
    return value;
}

void xdma_device::read_block(long addr, size_t size, void* buffer) {
    size_t num_bytes_read;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(control, addr, NULL, FILE_BEGIN)) {
        throw runtime_error("SetFilePointer failed: " + std::to_string(GetLastError()));
    }
    if (!ReadFile(control, buffer, (DWORD)size, (LPDWORD)&num_bytes_read, NULL)) {
        throw runtime_error("ReadFile failed:" + std::to_string(GetLastError()));
    }
}
*/
inline static uint32_t bit(uint32_t n) {
    return (1 << n);
}

inline static bool is_bit_set(uint32_t x, uint32_t n) {
    return (x & bit(n)) == bit(n);
}

class xdma_device {
public:
    xdma_device(const std::string& device_path);
    bool is_axi_st();
private:
    device_file control;
    uint32_t read_register(long addr);

};

xdma_device::xdma_device(const std::string& device_path) :
    control(device_path + "\\control", GENERIC_READ | GENERIC_WRITE)  {
}

uint32_t xdma_device::read_register(long addr) {
    uint32_t value = 0;
    control.seek(addr);
    size_t num_bytes_read = control.read(&value, sizeof(value));
    if (num_bytes_read <= 0) {
        throw std::runtime_error("Read returned zero bytes");
    }
    return value;
}

bool xdma_device::is_axi_st() {
    return is_bit_set(read_register(0x0), 15);
}
//获取设备路径
static std::vector<std::string> get_device_paths(GUID guid) {

    auto device_info = SetupDiGetClassDevs((LPGUID)&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (device_info == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("GetDevices INVALID_HANDLE_VALUE");
    }

    SP_DEVICE_INTERFACE_DATA device_interface = { 0 };
    device_interface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    // enumerate through devices

    std::vector<std::string> device_paths;

    for (unsigned index = 0;
        SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, index, &device_interface);
        ++index) {

        // get required buffer size
        unsigned long detailLength = 0;
        if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, NULL, 0, &detailLength, NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            throw std::runtime_error("SetupDiGetDeviceInterfaceDetail - get length failed");
        }

        // allocate space for device interface detail
        auto dev_detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(new char[detailLength]);
        dev_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        // get device interface detail
        if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, dev_detail, detailLength, NULL, NULL)) {
            delete[] dev_detail;
            throw std::runtime_error("SetupDiGetDeviceInterfaceDetail - get detail failed");
        }
        device_paths.emplace_back(dev_detail->DevicePath);
        delete[] dev_detail;
    }

    SetupDiDestroyDeviceInfoList(device_info);

    return device_paths;
}
