#include <iostream>
#include <complex>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <complex>
#include <chrono>

#include <iio.h>

#include "websocket_server.h"

WebSocketHandler* h_websocket; 
extern WebSocketHandler* init_websocket_server();

static std::string uri = "ip:192.168.2.1";
static size_t num_bins = 512;
static double rate = 60e6, freq=935e6, step=1e6, gain=50, bw=56e6, frame_rate=15;

static struct iio_context *ctx;
static struct iio_device *phy;
static struct iio_device *dev;
static struct iio_channel *rx0_i, *rx0_q, *rxch;
static struct iio_buffer *rxbuf;

namespace{/*anon*/    
    static const double pi = double(std::acos(-1.0));

    typedef std::vector<float> log_pwr_dft_type;

    template <typename T> std::complex<T> ct_fft_f(
        const std::complex<T> *samps, size_t nsamps,
        const std::complex<T> *factors,
        size_t start = 0, size_t step = 1
    ){
        if (nsamps == 1) return samps[start];
        std::complex<T> E_k = ct_fft_f(samps, nsamps/2, factors+1, start,      step*2);
        std::complex<T> O_k = ct_fft_f(samps, nsamps/2, factors+1, start+step, step*2);
        return E_k + factors[0]*O_k;
    }

    //! Compute an FFT for a particular bin k using Cooley-Tukey
    template <typename T> std::complex<T> ct_fft_k(
        const std::complex<T> *samps, size_t nsamps, size_t k
    ){
        //pre-compute the factors to use in Cooley-Tukey
        std::vector<std::complex<T> > factors;
        for (size_t N = nsamps; N != 0; N /= 2){
            factors.push_back(std::exp(std::complex<T>(0, T(-2*pi*k/N))));
        }
        return ct_fft_f(samps, nsamps, &factors.front());
    }

    template <typename T> log_pwr_dft_type log_pwr_dft(
        const std::complex<T> *samps, size_t nsamps
    ){
        if (nsamps & (nsamps - 1))
            throw std::runtime_error("num samps is not a power of 2");

        //compute the window
        double win_pwr = 0;
        std::vector<std::complex<T> > win_samps;
        for(size_t n = 0; n < nsamps; n++){
            //double w_n = 1;
            //double w_n = 0.54 //hamming window
            //    -0.46*std::cos(2*pi*n/(nsamps-1))
            //;
            double w_n = 0.35875 //blackman-harris window
                -0.48829*std::cos(2*pi*n/(nsamps-1))
                +0.14128*std::cos(4*pi*n/(nsamps-1))
                -0.01168*std::cos(6*pi*n/(nsamps-1))
            ;
            //double w_n = 1 // flat top window
            //    -1.930*std::cos(2*pi*n/(nsamps-1))
            //    +1.290*std::cos(4*pi*n/(nsamps-1))
            //    -0.388*std::cos(6*pi*n/(nsamps-1))
            //    +0.032*std::cos(8*pi*n/(nsamps-1))
            //;
            win_samps.push_back(T(w_n)*samps[n]);
            win_pwr += w_n*w_n;
        }

        //compute the log-power dft
        log_pwr_dft_type log_pwr_dft;
        for(size_t k = 0; k < nsamps; k++){
            std::complex<T> dft_k = ct_fft_k(&win_samps.front(), nsamps, k);
            log_pwr_dft.push_back(float(
                + 20*std::log10(std::abs(dft_k))
                - 20*std::log10(T(nsamps))
                - 10*std::log10(win_pwr/nsamps)
                + 3
            ));
        }

        return log_pwr_dft;
    }
}

void send_json_spectrum_params() {
    std::string json_spectrum_params = "{\"center\":" + std::to_string(freq);
    json_spectrum_params += ", \"span\":" + std::to_string(rate);
    json_spectrum_params += ", \"gain\":" + std::to_string(gain);
    json_spectrum_params += ", \"framerate\":" + std::to_string(frame_rate) + "}";

    h_websocket->process_param(const_cast<char*>(json_spectrum_params.c_str()));    
}

double extract_param(std::string param_str,std::string key) {
    if (param_str.find(key) != std::string::npos) {
        size_t pos1 = param_str.find(":");
        size_t pos2 = param_str.find("}");

        return std::stod(param_str.substr(pos1 + 1, pos2 - pos1 - 1));
    }
    else return -1;
}

void set_param(std::string key, double value) {
    
    if (key == "freq") {
        if (value < 70e6) value = 70e6;
        else if (value > 6e9) value = 6e9;

        freq = value;
        iio_channel_attr_write_longlong(iio_device_find_channel(phy, "altvoltage0", true), "frequency", freq);  // RX LO
    }
    else if (key == "span") {
        if (value < 3e6) value = 3e6; // avoid fractions
        else if (value > 61e6) value = 61e6;

        rate = value;
        iio_channel_attr_write_longlong(rxch, "sampling_frequency", rate);
    }
    else if (key == "bw") {
        if (value < 200e3) value = 200e3;
        else if (value > 56e6) value = 56e6;
        
        bw = value;
        iio_channel_attr_write_longlong(rxch, "rf_bandwidth", bw);
    }
    else if (key == "gain") {    
        if (value < 0) value = 0;
        else if (value >= 73) value = 73;

        gain = value;
        iio_channel_attr_write(rxch, "gain_control_mode", "manual"); // RX gain change only in manual mode
        iio_channel_attr_write_longlong(rxch,"hardwaregain", gain); // RX gain
    }
    else if (key == "fps") {
        if (value < 1) value = 1;
        else if (value < 10) value = 5;
        else if (value > 100) value = 100;

        frame_rate = value;
    }    
}

void check_update_params() {
    std::vector<std::string> update_params = h_websocket->get_update_params();
    double param_value;

    if (update_params.size()) {
        for (const auto& param_str : update_params) {
            if ((param_value = extract_param(param_str, "freq")) != -1)
                set_param("freq", param_value);
            else if ((param_value = extract_param(param_str, "span")) != -1)
                set_param("span", param_value);
            else if ((param_value = extract_param(param_str, "gain")) != -1)
                set_param("gain", param_value);
            else if ((param_value = extract_param(param_str, "fps")) != -1)
                set_param("fps", param_value);
        }
    }
}

void send_json_spectrum_data(std::vector<float> fftdata) {

        std::string json_spectrum_data;

        for (const auto& bin : fftdata)        
            json_spectrum_data += std::to_string(bin) + ",";

        json_spectrum_data.pop_back();
        json_spectrum_data = "{\"s\":[" + json_spectrum_data + "]}";

        //std::cout << json_data << std::endl;        

        h_websocket->process_param(const_cast<char*>(json_spectrum_data.c_str()));
}

static struct iio_context *scan(void)
{
    struct iio_scan_context *scan_ctx;
    struct iio_context_info **info;
    struct iio_context *ctx = NULL;
    unsigned int i;
    ssize_t ret;

    scan_ctx = iio_create_scan_context(NULL, 0);
    if (!scan_ctx) {
        fprintf(stderr, "Unable to create scan context\n");
        return NULL;
    }

    ret = iio_scan_context_get_info_list(scan_ctx, &info);
    if (ret < 0) {
        char err_str[1024];
        iio_strerror(-ret, err_str, sizeof(err_str));
        fprintf(stderr, "Scanning for IIO contexts failed: %s\n", err_str);
        goto err_free_ctx;
    }

    if (ret == 0) {
        printf("No IIO context found.\n");
        goto err_free_info_list;
    }

    if (ret == 1) {
        ctx = iio_create_context_from_uri(iio_context_info_get_uri(info[0]));
    } else {
        fprintf(stderr, "Multiple contexts found. Please select one using --uri:\n");

        for (i = 0; i < (size_t) ret; i++) {
            fprintf(stderr, "\t%d: %s [%s]\n", i,
                iio_context_info_get_description(info[i]),
                iio_context_info_get_uri(info[i]));
        }
    }

    err_free_info_list:
        iio_context_info_list_free(info);
    err_free_ctx:
        iio_scan_context_destroy(scan_ctx);

    return ctx;
}

static int init_device(void) {

    ssize_t ret;
    char attr_buf[1024];

    //create pluto device instance
    ctx = iio_create_context_from_uri(uri.c_str());
    if (!ctx) ctx = scan();

    if (!ctx) {
        fprintf(stderr, "Unable to create IIO context\n");
        return EXIT_FAILURE;
    }

    phy = iio_context_find_device(ctx,"ad9361-phy");

    if (!phy) {
        fprintf(stderr, "Device [ad9361-phy] not found\n");
        iio_context_destroy(ctx);
        return EXIT_FAILURE;
    }

    dev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    if (!dev) {
        fprintf(stderr, "Device [cf-ad9361-lpc] not found\n");
        iio_context_destroy(ctx);
        return EXIT_FAILURE;
    }

    rx0_i = iio_device_find_channel(dev, "voltage0", 0);
    rx0_q = iio_device_find_channel(dev, "voltage1", 0);

    rxch = iio_device_find_channel(phy, "voltage0", false);

    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);

    //set the sample rate
    std::cerr << "Setting RX Rate: " << (rate/1e6) << " Msps..." << std::endl;
    iio_channel_attr_write_longlong(rxch, "sampling_frequency",rate); // RX baseband sample rate

    ret = iio_channel_attr_read(rxch, "sampling_frequency", attr_buf, sizeof(attr_buf));
    if (ret > 0) std::cerr << ">> Actual RX Rate: " << (atol(attr_buf)/1e6) <<  " Msps..." << std::endl << std::endl;
    else std::cerr << "Error reading RX Rate" << std::endl << std::endl;

    //set the center frequency
    std::cerr << "Setting RX Freq: " << (freq/1e6) << " MHz..." << std::endl;
    iio_channel_attr_write_longlong(iio_device_find_channel(phy, "altvoltage0", true), "frequency", freq);  // RX LO

    ret = iio_channel_attr_read(iio_device_find_channel(phy, "altvoltage0", true), "frequency", attr_buf, sizeof(attr_buf));
    if (ret > 0) std::cerr << ">> Actual RX Freq:" << (atol(attr_buf)/1e6) << " MHz..." << std::endl << std::endl;
    else std::cerr << "Error reading RX Freq" << std::endl << std::endl;

    //set the rf gain
    std::cerr << "Setting RX Gain:" << gain << " dB..." << std::endl;
    iio_channel_attr_write(rxch, "gain_control_mode", "manual"); // RX gain change only in manual mode
    iio_channel_attr_write_longlong(rxch, "hardwaregain", gain); // RX gain

    ret = iio_channel_attr_read(rxch, "hardwaregain", attr_buf, sizeof(attr_buf));
    if (ret > 0) std::cerr << " >> Actual RX Gain:" << (atol(attr_buf)) << std::endl << std::endl;
    else std::cerr << "Error reading RX Gain" << std::endl << std::endl;

    //set the RF filter bandwidth
    std::cerr << "Setting RX Bandwidth:" << (bw/1e6) << " MHz..." << std::endl;
    iio_channel_attr_write_longlong( rxch, "rf_bandwidth", bw); // RF bandwidth

    ret = iio_channel_attr_read(rxch, "rf_bandwidth", attr_buf, sizeof(attr_buf));
    if (ret > 0) std::cerr << " >> Actual RX Bandwidth: " << (atol(attr_buf)/1e6) << " MHz ..." << std::endl << std::endl;
    else std::cerr << "Error reading RX Bandwidth" << std::endl << std::endl;

    rxbuf = iio_device_create_buffer(dev, num_bins, false);

    if (!rxbuf)
    {
        perror("Could not create RX buffer");
        iio_context_destroy(ctx);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void deinit_device(void){

    if (rxbuf) { iio_buffer_destroy(rxbuf); }

    if (rx0_i) { iio_channel_disable(rx0_i); }
    if (rx0_q) { iio_channel_disable(rx0_q); }

    if (rxch) { iio_channel_disable(rxch); }

    if (ctx) { iio_context_destroy(ctx); }
    
}

int main(int argc, char *argv[])
{
    bool loop = true;
    
    std::cerr << "SpectraSieve - Radio Spectrum Classifer v0.3b" << std::endl;        
    std::cerr << std::endl;
  
    if (init_device()) {
        std::cerr << "Error initializing device.." << std::endl << std::endl;
        deinit_device();
        return EXIT_FAILURE;
    }

    h_websocket = init_websocket_server();
        
    std::this_thread::sleep_for(std::chrono::seconds(1)); //allow for some setup time

    std::vector<std::complex<float> > buff(num_bins);  

    //------------------------------------------------------------------
    //-- Initialize
    //------------------------------------------------------------------

    auto next_refresh = std::chrono::high_resolution_clock::now();

    void *p_dat, *p_end, *t_dat;
    ptrdiff_t p_inc;
    float i,q;

    //------------------------------------------------------------------
    //-- Main loop
    //------------------------------------------------------------------

    log_pwr_dft_type last_lpdft;

    while (loop){

        buff.clear();

        iio_buffer_refill(rxbuf);

        p_inc = iio_buffer_step(rxbuf);
        p_end = iio_buffer_end(rxbuf);

        for(p_dat = iio_buffer_first(rxbuf, rx0_i); p_dat < p_end; p_dat += p_inc, t_dat += p_inc)
        {
            i = ((float)((int16_t*)p_dat)[0])/2048;
            q = ((float)((int16_t*)p_dat)[1])/2048;

            buff.push_back(std::complex<float> ( i,  q ));
        }

        //check and update the display refresh condition
        if (std::chrono::high_resolution_clock::now() < next_refresh) {
            continue;
        }
        next_refresh =
            std::chrono::high_resolution_clock::now()
            + std::chrono::microseconds(int64_t(1e6/frame_rate));
        
        //calculate the dft 
        log_pwr_dft_type lpdft(
            log_pwr_dft(&buff.front(), buff.size())
        );

        const size_t num_bins_dc = lpdft.size() - 1 + lpdft.size() % 2; //make it odd
        log_pwr_dft_type lpdft_dc(num_bins_dc);

        // dc in center
        for (size_t n = 0; n < num_bins_dc; n++){
            lpdft_dc[n] = lpdft[(n + num_bins_dc/2) % num_bins_dc];            
        }   

        send_json_spectrum_params();
        send_json_spectrum_data(lpdft_dc);
        check_update_params();

        //std::copy(lpdft_dc.begin(), lpdft_dc.end() - 1, std::ostream_iterator<float>(std::cout, ","));
        //std::cout << lpdft_dc.back() << std::endl;
        //std::cout << std::endl;
    }

    //------------------------------------------------------------------
    //-- Cleanup
    //------------------------------------------------------------------

    deinit_device();    
    return EXIT_SUCCESS;
}

