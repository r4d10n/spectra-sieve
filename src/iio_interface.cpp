#include "iio_interface.h"
#include <iostream>
#include <cstdlib>

iio_interface::iio_interface() : ctx(nullptr), phy(nullptr), dev(nullptr), 
    rx0_i(nullptr), rx0_q(nullptr), rxch(nullptr), rxbuf(nullptr), num_bins(512) {}

iio_interface::~iio_interface() {
    deinit_device();
}

int iio_interface::init_device(const std::string& uri, double rate, double freq, double gain, double bw) {
    ctx = iio_create_context_from_uri(uri.c_str());
    if (!ctx) ctx = scan();

    if (!ctx) {
        std::cerr << "Unable to create IIO context" << std::endl;
        return EXIT_FAILURE;
    }

    phy = iio_context_find_device(ctx, "ad9361-phy");
    if (!phy) {
        std::cerr << "Device [ad9361-phy] not found" << std::endl;
        iio_context_destroy(ctx);
        return EXIT_FAILURE;
    }

    dev = iio_context_find_device(ctx, "cf-ad9361-lpc");
    if (!dev) {
        std::cerr << "Device [cf-ad9361-lpc] not found" << std::endl;
        iio_context_destroy(ctx);
        return EXIT_FAILURE;
    }

    rx0_i = iio_device_find_channel(dev, "voltage0", 0);
    rx0_q = iio_device_find_channel(dev, "voltage1", 0);
    rxch = iio_device_find_channel(phy, "voltage0", false);

    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);

    set_sample_rate(rate);
    set_frequency(freq);
    set_gain(gain);
    set_bandwidth(bw);

    rxbuf = iio_device_create_buffer(dev, num_bins, false);
    if (!rxbuf) {
        std::cerr << "Could not create RX buffer" << std::endl;
        iio_context_destroy(ctx);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void iio_interface::deinit_device() {
    if (rxbuf) { iio_buffer_destroy(rxbuf); }
    if (rx0_i) { iio_channel_disable(rx0_i); }
    if (rx0_q) { iio_channel_disable(rx0_q); }
    if (rxch) { iio_channel_disable(rxch); }
    if (ctx) { iio_context_destroy(ctx); }
}

int iio_interface::refill_buffer() {
    return iio_buffer_refill(rxbuf);
}

ssize_t iio_interface::get_buffer_step() {
    return iio_buffer_step(rxbuf);
}

void* iio_interface::get_buffer_end() {
    return iio_buffer_end(rxbuf);
}

void* iio_interface::get_first_channel() {
    return iio_buffer_first(rxbuf, rx0_i);
}

void iio_interface::set_frequency(long long freq) {
    iio_channel_attr_write_longlong(iio_device_find_channel(phy, "altvoltage0", true), "frequency", freq);
}

void iio_interface::set_gain(long long gain) {
    iio_channel_attr_write(rxch, "gain_control_mode", "manual");
    iio_channel_attr_write_longlong(rxch, "hardwaregain", gain);
}

void iio_interface::set_sample_rate(long long rate) {
    iio_channel_attr_write_longlong(rxch, "sampling_frequency", rate);
}

void iio_interface::set_bandwidth(long long bw) {
    iio_channel_attr_write_longlong(rxch, "rf_bandwidth", bw);
}

struct iio_context* iio_interface::scan() {
    struct iio_scan_context *scan_ctx;
    struct iio_context_info **info;
    struct iio_context *ctx = nullptr;
    ssize_t ret;

    scan_ctx = iio_create_scan_context(nullptr, 0);
    if (!scan_ctx) {
        std::cerr << "Unable to create scan context" << std::endl;
        return nullptr;
    }

    ret = iio_scan_context_get_info_list(scan_ctx, &info);
    if (ret < 0) {
        char err_str[1024];
        iio_strerror(-ret, err_str, sizeof(err_str));
        std::cerr << "Scanning for IIO contexts failed: " << err_str << std::endl;
    } else if (ret == 0) {
        std::cout << "No IIO context found." << std::endl;
    } else if (ret == 1) {
        ctx = iio_create_context_from_uri(iio_context_info_get_uri(info[0]));
    } else {
        std::cerr << "Multiple contexts found. Please select one using --uri:" << std::endl;
        for (unsigned int i = 0; i < (size_t)ret; i++) {
            std::cerr << "\t" << i << ": " << iio_context_info_get_description(info[i])
                      << " [" << iio_context_info_get_uri(info[i]) << "]" << std::endl;
        }
    }

    iio_context_info_list_free(info);
    iio_scan_context_destroy(scan_ctx);

    return ctx;
}