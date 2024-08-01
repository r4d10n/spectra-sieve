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
#include <iomanip>

#include "fft_ops.h"
#include "iio_interface.h"
#include "websocket_server.h"

WebSocketHandler* h_websocket; 
extern WebSocketHandler* init_websocket_server();

static std::string uri = "ip:192.168.2.1";
static size_t num_bins = 512;
static double step=1e6, frame_rate=15;
static double span, wfrows;

iio_interface iio_intf;
double rate = 60e6, freq = 935e6, gain = 50, bw = 56e6;
    
void send_json_spectrum_params() {
    std::string json_spectrum_params = "{\"center\":" + std::to_string(freq);
    json_spectrum_params += ", \"span\":" + std::to_string(span);
    json_spectrum_params += ", \"gain\":" + std::to_string(gain);
    json_spectrum_params += ", \"framerate\":" + std::to_string(frame_rate);
    json_spectrum_params += ", \"wfrows\":" + std::to_string(wfrows) + "}";

    h_websocket->process_param(const_cast<char*>(json_spectrum_params.c_str()));    
}

double extract_param(std::string param_str,std::string key) {
    if (param_str.find(key) != std::string::npos) {
        size_t pos1 = param_str.find(":");
        size_t pos2 = param_str.find("}");
        std::string param_val = param_str.substr(pos1 + 1, pos2 - pos1 - 1);
        
        if (param_val.size() > 0)
            return std::stod(param_val);
        else
            return -1;
    }
    else return -1;
}

void set_intf_param(std::string key, double value) {
    if (key == "freq") {
        if (value < 70e6) value = 70e6;
        else if (value > 6e9) value = 6e9;

        freq = value;
        iio_intf.set_frequency(freq);
        std::cerr << "Setting RX Freq: " << (freq/1e6) << " MHz..." << std::endl;
    }
    else if (key == "rate") {
        if (value < 3e6) value = 3e6; // avoid fractions
        else if (value > 61e6) value = 61e6;

        rate = value;
        iio_intf.set_sample_rate(rate);
        std::cerr << "Setting RX Rate: " << (rate/1e6) << " Msps..." << std::endl;
    }
    else if (key == "bw") {
        if (value < 200e3) value = 200e3;
        else if (value > 56e6) value = 56e6;
        
        bw = value;
        iio_intf.set_bandwidth(bw);
        std::cerr << "Setting RX Bandwidth: " << (bw/1e6) << " MHz..." << std::endl;
    }
    else if (key == "gain") {    
        if (value < 0) value = 0;
        else if (value >= 73) value = 73;

        gain = value;
        iio_intf.set_gain(gain);
        std::cerr << "Setting RX Gain: " << gain << " dB..." << std::endl;
    }
    else if (key == "fps") {
        if (value < 1) value = 1;
        else if (value < 10) value = 5;
        else if (value > 100) value = 100;

        frame_rate = value;
        std::cerr << "Setting frame rate: " << frame_rate << " fps..." << std::endl;
    } 
    else if (key == "span") {   
        span = value;
        std::cerr << "Setting span: " << (span/1e6) << " MHz..." << std::endl;
    }    

    send_json_spectrum_params();
}

void check_update_params() {
    std::vector<std::string> update_params = h_websocket->get_update_params();
    double param_value;

    if (update_params.size()) {
        for (const auto& param_str : update_params) {
            if ((param_value = extract_param(param_str, "freq")) != -1)
                set_intf_param("freq", param_value);
            else if ((param_value = extract_param(param_str, "span")) != -1)
                set_intf_param("span", param_value);
            else if ((param_value = extract_param(param_str, "gain")) != -1)
                set_intf_param("gain", param_value);
            else if ((param_value = extract_param(param_str, "fps")) != -1)
                set_intf_param("fps", param_value);
            else if ((param_value = extract_param(param_str, "rate")) != -1)
                set_intf_param("rate", param_value);
            else if ((param_value = extract_param(param_str, "bw")) != -1)
                set_intf_param("bw", param_value);
        }
    }
}
void send_json_spectrum_data(std::vector<float> fftdata) {

        // std::string json_spectrum_data;

        // for (const auto& bin : fftdata)        
        //     json_spectrum_data += std::to_string(bin) + ",";

        // json_spectrum_data.pop_back();

        // json_spectrum_data = "{\"s\":[" + json_spectrum_data + "]}";

        h_websocket->process_data(fftdata.data(), fftdata.size());
}


int main(int argc, char *argv[])
{
   
    bool loop = true;
    
    std::cerr << "SpectraSieve - Radio Spectrum Classifer v0.3b" << std::endl;        
    std::cerr << std::endl;
      
    if (iio_intf.init_device(uri, rate, freq, gain, bw)) {
        std::cerr << "Error initializing device.." << std::endl << std::endl;
        return EXIT_FAILURE;
    }

    h_websocket = init_websocket_server();
        
    std::this_thread::sleep_for(std::chrono::seconds(1)); //allow for some setup time

    std::vector<std::complex<float>> buff(num_bins);  

    //------------------------------------------------------------------
    //-- Initialize
    //------------------------------------------------------------------

    auto next_refresh = std::chrono::high_resolution_clock::now();

    char *p_dat, *p_end, *t_dat;
    ptrdiff_t p_inc;
    float i,q;

    //------------------------------------------------------------------
    //-- Main loop
    //------------------------------------------------------------------

    std::vector<float> swept_bins;
    int refresh_counter = 0;
    int count_settle = 0;

    double sweep_freq_start = 800e6, sweep_freq_end = 1040e6, sweep_jump = 60e6;
    double sweep_freq = sweep_freq_start + (sweep_jump/2);    

    freq = (sweep_freq_start + sweep_freq_end)/2;
    span = sweep_freq_end - sweep_freq_start;
    wfrows = (span / sweep_jump) * num_bins;
    swept_bins.reserve(wfrows);

    iio_intf.set_frequency(sweep_freq);

    count_settle = 0;
    while(count_settle < 100) {
        iio_intf.refill_buffer();
        count_settle++;        
    }

    while (loop){

        buff.clear();
        iio_intf.refill_buffer();

        p_inc = iio_intf.get_buffer_step();
        p_end = static_cast<char*>(iio_intf.get_buffer_end());

        for(p_dat = static_cast<char*>(iio_intf.get_first_channel()); 
            p_dat < p_end; 
            p_dat += p_inc, t_dat += p_inc)
        {
            i = ((float)(reinterpret_cast<int16_t*>(p_dat)[0]))/2048;
            q = ((float)(reinterpret_cast<int16_t*>(p_dat)[1]))/2048;

            buff.push_back(std::complex<float> ( i,  q ));
        }
       
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

        //std::copy(lpdft_dc.begin(), lpdft_dc.end() - 1, std::ostream_iterator<float>(std::cout, ","));

        swept_bins.insert(swept_bins.end(),lpdft_dc.begin(), lpdft_dc.end());

        refresh_counter = 0;

        if (sweep_freq < (sweep_freq_end - (sweep_jump/2))) 
        {
            sweep_freq += (sweep_jump);
        }
        else
        {
            sweep_freq = sweep_freq_start + (sweep_jump/2);
            send_json_spectrum_params();
            send_json_spectrum_data(swept_bins);
            check_update_params();            

            //fprintf(stderr,"--------\nSweep Freq: %f | Swept Bins: %d | Sw Begin: %d | Sw End: %d | Lp Begin: %d | Lp End: %d\n", sweep_freq/1e6, swept_bins.size(), swept_bins.begin(), swept_bins.end(), lpdft_dc.begin(), lpdft_dc.end());
            swept_bins.resize(0);               
        }
        
        iio_intf.set_frequency(sweep_freq);

        count_settle = 0;
        while(count_settle < 100) {
            iio_intf.refill_buffer();
            count_settle++;        
        }
    }

        // std::copy(lpdft_dc.begin(), lpdft_dc.end() - 1, std::ostream_iterator<float>(std::cout, ","));
        // std::cout << lpdft_dc.back() << std::endl;
        // std::cout << std::endl;

    iio_intf.deinit_device();   
        
}

    