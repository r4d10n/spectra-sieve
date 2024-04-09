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

int main(int argc, char *argv[])
{

    std::string uri = "ip:192.168.2.1";
    size_t num_bins = 512;
    double rate = 60e6, freq=935e6, step=1e6, gain=50, bw=56e6, frame_rate=15;
    float ref_lvl=0, dyn_rng=80;
    
    ssize_t ret;
    char attr_buf[1024];

    int ch;
    bool loop = true;
    
    std::cerr << "PlutoStream - Stream samples from PlutoSDR" << std::endl;
    
    //create pluto device instance
    std::cerr << std::endl;
    
    struct iio_context *ctx;

    ctx = iio_create_context_from_uri(uri.c_str());
    if (!ctx) ctx = scan();

    if (!ctx) {
        fprintf(stderr, "Unable to create IIO context\n");
        return EXIT_FAILURE;
    }

    struct iio_device *phy;
    struct iio_device *dev;
    struct iio_channel *rx0_i, *rx0_q, *rxch;
    struct iio_buffer *rxbuf;


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

    std::this_thread::sleep_for(std::chrono::seconds(1)); //allow for some setup time

    std::vector<std::complex<float> > buff(num_bins);

    rxbuf = iio_device_create_buffer(dev, num_bins, false);

    if (!rxbuf)
    {
        perror("Could not create RX buffer");
        iio_context_destroy(ctx);
        return EXIT_FAILURE;
    }

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
        
        //calculate the dft and create the ascii art frame
        log_pwr_dft_type lpdft(
            log_pwr_dft(&buff.front(), buff.size())
        );

        const size_t num_bins_dc = lpdft.size() - 1 + lpdft.size()%2; //make it odd
        log_pwr_dft_type lpdft_dc(num_bins_dc);

        // dc in center
        for (size_t n = 0; n < num_bins_dc; n++){
            lpdft_dc[n] = lpdft[(n + num_bins_dc/2)%num_bins_dc];            
        }               

        std::copy(lpdft_dc.begin(), lpdft_dc.end() - 1, std::ostream_iterator<float>(std::cout, ","));
        std::cout << lpdft_dc.back() << std::endl;
        //std::cout << std::endl;
    }

    //------------------------------------------------------------------
    //-- Cleanup
    //------------------------------------------------------------------

    if (rxbuf) { iio_buffer_destroy(rxbuf); }

    if (rx0_i) { iio_channel_disable(rx0_i); }
    if (rx0_q) { iio_channel_disable(rx0_q); }

    if (rxch) { iio_channel_disable(rxch); }

    if (ctx) { iio_context_destroy(ctx); }
    
    //finished
    std::cerr << std::endl << (char)(ch) << std::endl << "Done!" << std::endl << std::endl;

    return EXIT_SUCCESS;
}

/*
void set_params() {
        switch(ch)
        {
            case 'r':
            {
                ret = iio_channel_attr_read(rxch, "sampling_frequency", attr_buf, sizeof(attr_buf));
                if (ret > 0)
                {
                    rate = atof(attr_buf);

                    if ((rate - step) < 3e6) rate = 3e6; // avoid fractions
                    else rate -= step;

                    iio_channel_attr_write_longlong(rxch, "sampling_frequency", rate);
                }
                break;
            }

            case 'R':
            {
                ret = iio_channel_attr_read(rxch, "sampling_frequency", attr_buf, sizeof(attr_buf));
                if (ret > 0)
                {
                    rate = atof(attr_buf);

                    if ((rate + step) > 61e6) rate = 61e6; // avoid fractions
                    else rate += step;

                    iio_channel_attr_write_longlong(rxch, "sampling_frequency", rate);
                }
                break;
            }

            case 'b':
            {
                ret = iio_channel_attr_read(rxch, "rf_bandwidth", attr_buf, sizeof(attr_buf));
                if (ret > 0)
                {
                    bw = atof(attr_buf);

                    if ((bw - step)  < 200e3) bw = 200e3;
                    else  bw -= step;

                    iio_channel_attr_write_longlong(rxch, "rf_bandwidth", bw);
                }
                break;
            }

            case 'B':
            {
                ret = iio_channel_attr_read(rxch, "rf_bandwidth", attr_buf, sizeof(attr_buf));
                if (ret > 0)
                {
                    bw = atof(attr_buf);

                    if ((bw + step) > 56e6) bw = 56e6;
                    else bw += step;

                    iio_channel_attr_write_longlong(rxch, "rf_bandwidth", bw);
                }
                break;
            }

            case 'g':
            {
                ret = iio_channel_attr_read(rxch, "hardwaregain", attr_buf, sizeof(attr_buf));
                if (ret > 0)
                {
                    gain = atof(attr_buf);

                    if (gain >= 0) gain -= 1;

                    iio_channel_attr_write(rxch, "gain_control_mode", "manual"); // RX gain change only in manual mode
                    iio_channel_attr_write_longlong(rxch,"hardwaregain", gain); // RX gain
                }
                break;
            }

            case 'G':
            {
                ret = iio_channel_attr_read(rxch, "hardwaregain", attr_buf, sizeof(attr_buf));
                if (ret > 0)
                {
                    gain = atof(attr_buf);

                    if (gain <= 72) gain += 1;

                    iio_channel_attr_write(rxch, "gain_control_mode", "manual"); // RX gain change only in manual mode
                    iio_channel_attr_write_longlong(rxch,"hardwaregain", gain); // RX gain
                }
                break;
            }

            case 'f':
            {
                ret = iio_channel_attr_read(iio_device_find_channel(phy, "altvoltage0", true), "frequency", attr_buf, sizeof(attr_buf));

                if (ret > 0)
                {
                    freq = atof(attr_buf);

                    if ((freq - step) < 70e6) freq = 70e6;
                    else freq -= step;

                    iio_channel_attr_write_longlong(iio_device_find_channel(phy, "altvoltage0", true), "frequency", freq);  // RX LO
                }
                break;
            }

            case 'F':
            {
                ret = iio_channel_attr_read(iio_device_find_channel(phy, "altvoltage0", true), "frequency", attr_buf, sizeof(attr_buf));

                if (ret > 0)
                {
                    freq = atof(attr_buf);

                    if ((freq + step) > 6e9) freq = 6e9;
                    else freq += step;

                    iio_channel_attr_write_longlong(iio_device_find_channel(phy, "altvoltage0", true), "frequency", freq);  // RX LO
                }
                break;
            }
            
            case 'h': { peak_hold = false; break; }
            case 'H': { peak_hold = true; break; }
            case 'l': { ref_lvl -= 10; break; }
            case 'L': { ref_lvl += 10; break; }
            case 'd': { dyn_rng -= 10; break; }
            case 'D': { dyn_rng += 10; break; }
            case 's': { if (frame_rate > 1) frame_rate -= 1; break;}
            case 'S': { frame_rate += 1; break; }
            case 'n': { if (num_bins > 2) num_bins /= 2; break;}
            case 'N': { num_bins *= 2; break; }
            case 't': { if (step > 1) step /= 2; break; }
            case 'T': { step *= 2; break; }
            case 'c': { show_controls = false; break; }
            case 'C': { show_controls = true; break; }

            case 'q':
            case 'Q': { loop = false; break; }
        }

        // Arrow keys handling: '\033' '[' 'A'/'B'/'C'/'D' -- Up / Down / Right / Left Press
        if (ch == '\033')
        {
            getch();
            switch(getch())
            {
		        case 'A':
                case 'C':
                    ret = iio_channel_attr_read(iio_device_find_channel(phy, "altvoltage0", true), "frequency", attr_buf, sizeof(attr_buf));

                    if (ret > 0)
                    {
                        freq = atof(attr_buf);
                        if (freq >= 6e9 || freq <= 70e6) continue;

                        freq += step;

                        iio_channel_attr_write_longlong(iio_device_find_channel(phy, "altvoltage0", true), "frequency", freq);  // RX LO
                    }

                    break;

		        case 'B':
                case 'D':
                    ret = iio_channel_attr_read(iio_device_find_channel(phy, "altvoltage0", true), "frequency", attr_buf, sizeof(attr_buf));

                    if (ret > 0)
                    {
                        freq = atof(attr_buf);
                        if (freq >= 6e9 || freq <= 70e6) continue;

                        freq -= step;

                        iio_channel_attr_write_longlong(iio_device_find_channel(phy, "altvoltage0", true), "frequency", freq);  // RX LO
                    }

                    break;
        }
}*/
