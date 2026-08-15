#include "camera_driver.hpp"

namespace camera_driver
{
    CameraDriver::CameraDriver(rclcpp::NodeOptions const & options)
    : rclcpp::Node{"camera_driver", options}, logger_{this->get_logger()}
    {
        this->declare_parameter("use_gpu", false);
        this->declare_parameter("use_opencv", false);
        this->declare_parameter("measure_latency", false);
        this->declare_parameter("device_file", "/dev/video0");
        this->declare_parameter("camera_frame_id", "camera");
        this->declare_parameter("image_publish_hz", 10);
        this->declare_parameter("info_publish_hz", 1);
        this->declare_parameter("n_buffers", 4);
        this->declare_parameter("original_height", 656);
        this->declare_parameter("original_width", 1184);


        if(!onInitialize()){
            RCLCPP_ERROR(logger_, "Failed Initialized");
            throw std::runtime_error("Failed Initialized");
        }

        RCLCPP_INFO(logger_, "use_sim_time : %d", static_cast<int>(use_sim_time_));
        RCLCPP_INFO(logger_, "use_gpu : %d", static_cast<int>(use_gpu_));
        RCLCPP_INFO(logger_, "image callback time : %d ms", image_freqtotime_);
        RCLCPP_INFO(logger_, "info callback time : %d ms", info_freqtotime_);

        counter_ = 0;
        success_flg_ = false;

        image_timer_ = this->create_wall_timer(std::chrono::milliseconds(image_freqtotime_),
            std::bind(&CameraDriver::onFastUpdate, this));
        info_timer_ = this->create_wall_timer(std::chrono::milliseconds(info_freqtotime_),
            std::bind(&CameraDriver::onSlowUpdate, this));

    }

    CameraDriver::~CameraDriver()
    {
        freeCUDA();

        if(fd_ >= 0){
            if(stop()){
                RCLCPP_INFO(logger_, "Stop Camera Capture");
            } else {
                RCLCPP_ERROR(logger_, "Failed Stop Camera Capture");
            }
            ::close(fd_);
        }
    }

    bool CameraDriver::onInitialize()
    {
        bool ret_ros = false;
        bool ret_device = false;
        bool ret = false;
        ret_ros = onParameterInitialize() && onPublisherInitialize() 
            && onOpenCVInitialize();
            
        ret_device = onDeviceInitialize() && onFormatInitialize() 
            && onRequestBufferInitialize() && onMemoryMapInitialize() && onStreamingInitialize();

        ret = ret_ros && ret_device;

        return ret;
    }

    bool CameraDriver::onDeviceInitialize()
    {
        bool ret = false;

        // ::はグローバル名前空間にある関数を呼び出すという意味
        // 読み取りやデキュー操作で executor / タイマーコールバックがブロックしないよう、
        // デバイスをノンブロッキングで開きます。
        // ノンブロッキングモードではフレームがない場合に `VIDIOC_DQBUF` が `-EAGAIN` を返します。
        // 補足: ノンブロッキングにしたため、DQBUF が EAGAIN を返すケースを静かに無視する
        // 処理を実装。これによりタイマーが長時間ブロックされずに済む
        fd_ = ::open(device_file_.c_str(), O_RDWR | O_NONBLOCK);
        RCLCPP_INFO(logger_, "open device : %s", device_file_.c_str());
        if(fd_ < 0){
            RCLCPP_ERROR(logger_, "failed opening device %s", device_file_.c_str());
            return ret;
        }

        struct v4l2_capability cap;
        // ここでデバイスのcapability(権限の組み合わせ)について問い合わせる
        // https://qiita.com/Brutus/items/37d942214b4c6edd08df
        if(xioctl(fd_, VIDIOC_QUERYCAP, &cap) == -1){
            RCLCPP_ERROR(logger_, "QUERYCAP");
            return ret;
        }

        // 対応しているかの確認
        if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
            RCLCPP_ERROR(logger_, "no video capture");
            return ret;
        }

        if(!(cap.capabilities & V4L2_CAP_STREAMING)){
            RCLCPP_ERROR(logger_, "does not support stream");
            return ret;
        }

        ret = true;

        return ret;
    }

    bool CameraDriver::onFormatInitialize()
    {
        bool ret = false;

        fmt_ = {};
        fmt_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        // デフォルトwidth*height = 1184*656
        fmt_.fmt.pix.width = original_width_;
        fmt_.fmt.pix.height = original_height_;
        fmt_.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt_.fmt.pix.field = V4L2_FIELD_NONE;

        if(xioctl(fd_, VIDIOC_S_FMT, &fmt_) == -1){
            RCLCPP_ERROR(logger_, "VIDIOC_S_FMT");
            return ret;
        }

        memset(&fmt_, 0, sizeof(fmt_));
        fmt_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(xioctl(fd_, VIDIOC_G_FMT, &fmt_) == -1){
            RCLCPP_ERROR(logger_, "VIDIOC_G_FMT");
            return ret;
        }

        if(fmt_.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV){
            RCLCPP_ERROR(logger_, "does not support YUYV");
            return ret;
        }

        if(fmt_.fmt.pix.width != 1184 || fmt_.fmt.pix.height != 656){
            RCLCPP_ERROR(logger_, "does not support %dx%d", original_width_, original_height_);
            return ret;
        }

        ret = true;
        return ret;
    }

    bool CameraDriver::onRequestBufferInitialize()
    {
        bool ret = false;

        req_ = {};

        // バッファリングするフレーム数
        req_.count = n_buffers_;
        // バッファの種類
        req_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        // メモリの種類
        req_.memory = V4L2_MEMORY_MMAP;
        if(xioctl(fd_, VIDIOC_REQBUFS, &req_) == -1){
            RCLCPP_ERROR(logger_, "VIDIOC_REQBUFS");
            return ret;
        }

        // 確保できた枚数の確認
        if(req_.count < n_buffers_){
            RCLCPP_ERROR(logger_, "Insufficient buffer memory");
            return ret;
        }


        ret = true;
        return ret;
    }

    bool CameraDriver::onMemoryMapInitialize()
    {
        bool ret = false;

        // 動的にメモリ領域を割当、割り当てた領域を0で初期化する(mallocとmemsetを同時に行うイメージ)
        buffers_ = (struct Buffer*)calloc(n_buffers_, sizeof(*buffers_));
    
        if(buffers_ == NULL){
            RCLCPP_ERROR(logger_, "Out of memory");
            return ret;
        }

        // バッファの数だけループして、各バッファの情報を取得する
        // mmap関数を使用して、カーネル空間のバッファをユーザー空間にマッピングする
        // bufferにいっぱいになるまで画像を格納するようデバイスに指示する
        for(size_t i = 0; i < req_.count; ++i){
            struct v4l2_buffer buff;

            memset(&buff, 0, sizeof(buff));
            buff.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buff.memory = V4L2_MEMORY_MMAP;
            buff.index = i;

            if(xioctl(fd_, VIDIOC_QUERYBUF, &buff) == -1){
                RCLCPP_ERROR(logger_, "VIDIOC_QUERYBUF");
                return ret;
            }

            buffers_[i].index = buff.index;
            buffers_[i].length = buff.length;
            buffers_[i].start = mmap(NULL, buff.length, 
                PROT_READ | PROT_WRITE, 
                MAP_SHARED, 
                fd_, 
                buff.m.offset);

            if(buffers_[i].start == MAP_FAILED){
                RCLCPP_ERROR(logger_, "mmap error");
                return ret;
            }
            // ストリーミング開始前にドライバがバッファを埋められるよう、各バッファをキューします
            // 補足: V4L2 の MMAP + QBUF/DQBUF フローでは、STREAMON の前に QBUF を行い
            // ドライバに空きバッファを渡しておく必要があります。これを行わないとドライバが書き込む
            // 先が無くなり、フレーム取得に失敗します。
            if(xioctl(fd_, VIDIOC_QBUF, &buff) == -1){
                RCLCPP_ERROR(logger_, "VIDIOC_QBUF");
                return ret;
            }
        }

        ret = true;
        return ret;
    }

    bool CameraDriver::onStreamingInitialize()
    {
        bool ret = false;

        enum v4l2_buf_type type;
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(xioctl(fd_, VIDIOC_STREAMON, &type) == -1){
            RCLCPP_ERROR(logger_, "stream on error");
            return ret;
        }

        ret = true;
        return ret;
    }

    bool CameraDriver::onParameterInitialize()
    {
        bool ret = false;

        use_sim_time_ = this->get_parameter("use_sim_time").as_bool();
        use_gpu_ = this->get_parameter("use_gpu").as_bool();
        use_opencv_ = this->get_parameter("use_opencv").as_bool();
        measure_latency_ = this->get_parameter("measure_latency").as_bool();

        device_file_ = this->get_parameter("device_file").as_string();
        camera_frame_id_ = this->get_parameter("camera_frame_id").as_string();
        image_publish_frequency_ = this->get_parameter("image_publish_hz").as_int();
        info_publish_frequency_ = this->get_parameter("info_publish_hz").as_int();
        n_buffers_ = static_cast<std::uint32_t>(this->get_parameter("n_buffers").as_int());
        original_height_ = this->get_parameter("original_height").as_int();
        original_width_ = this->get_parameter("original_width").as_int();

        image_freqtotime_ = 1000 / image_publish_frequency_;
        info_freqtotime_ = 1000 / info_publish_frequency_;

        ret = true;
        return ret;
    }

    bool CameraDriver::onPublisherInitialize()
    {
        bool ret = false;

        image_pub_ = this->create_publisher<sensor_msgs::msg::Image>("image_raw", 10);
        info_pub_ = this->create_publisher<sensor_msgs::msg::CameraInfo>("camera_info", 10);

        ret = true;
        return ret;
    }

    bool CameraDriver::onOpenCVInitialize()
    {
        bool ret = false;

        ret = true;
        return ret;
    }

    void CameraDriver::onFastUpdate()
    {
        bool ret_capture = false;

        sensor_msgs::msg::Image::SharedPtr image_msg 
            = std::make_shared<sensor_msgs::msg::Image>();

        ret_capture = capture();
        if(ret_capture){
            if(setImageToROSMessage(image_msg)){
                (void)publishImageMessage(image_msg);
                if(!success_flg_) {
                    RCLCPP_INFO(logger_, "success capture");
                    success_flg_ = true;
                }
            } else {
                RCLCPP_WARN(logger_, "failed set ros message");
                success_flg_ = false;
            }
        } else {
            RCLCPP_WARN(logger_, "failed capture");
            success_flg_ = false;
        }
    }

    void CameraDriver::onSlowUpdate()
    {
    }

    bool CameraDriver::capture()
    {
        bool ret = false;
        struct pollfd fds[1];

        fds[0].fd = fd_;
        fds[0].events = POLLIN;
        // タイマーコールバックをブロックさせないよう、ノンブロッキングの poll（タイムアウト0）を使用します
        // 補足: ここで短時間または即時リターンにしておくことで、SingleThreadedExecutor 上の
        // 他のコールバック（タイマーやサービス等）が阻害されないようにしています。
        int p = poll(fds, 1, 0);

        if(p == -1){
            RCLCPP_WARN(logger_, "poll error waiting for frame");
            return ret;
        }

        if(p == 0){
            // no data available right now
            return ret;
        }

        // p > 0 and data available
        ret = true;
        return ret;
    }

    void CameraDriver::yuyv422_to_rgba8(const std::uint8_t * yuyv, size_t yuyv_size, std::vector<std::uint8_t>& rgba)
    {
        if(use_gpu_){
            if(checkCUDA()){
                std::optional<std::string> error_string = std::nullopt;
                launchYUV422ToRGBA8_CUDAKernel(yuyv, yuyv_size, rgba, original_width_, original_height_, error_string);
                if(error_string){
                    RCLCPP_ERROR(logger_, "CUDA Error");
                }
            } else {
                RCLCPP_WARN(logger_, "Cannot get valid device");
                counter_++;
                if(counter_ > 10) use_gpu_ = false;
            }
        } else {
            yuyv422_to_rgba8_cpu(yuyv, rgba);
        }
    }

    void CameraDriver::yuyv422_to_rgba8_cpu(const std::uint8_t * yuyv, std::vector<std::uint8_t>& rgba)
    {
        if(use_opencv_){
            // 直接ポインタを cv::Mat に渡す（const_cast が必要）
            cv::Mat src(original_height_, original_width_, CV_8UC2, const_cast<std::uint8_t*>(yuyv));
            cv::Mat dst(original_height_, original_width_, CV_8UC4, rgba.data());
            cv::cvtColor(src, dst, cv::COLOR_YUV2RGBA_YUY2);
        } else {
            int num_blocks = (original_height_*original_width_) / 2;

            for(int i = 0; i < num_blocks; ++i){
                int yuv_idx = i * 4;
                int rgba_idx = i * 8;

                int y0 = *(yuyv + yuv_idx + 0);
                int u  = *(yuyv + yuv_idx + 1) - 128;
                int y1 = *(yuyv + yuv_idx + 2);
                int v  = *(yuyv + yuv_idx + 3) - 128;

                int r_coeff = (11585 * v) >> 13;
                int g_coeff = ((-2901 * u) - (5902 * v)) >> 13;
                int b_coeff = (14624 * u) >> 13;

                *(rgba.data() + rgba_idx + 0) = static_cast<std::uint8_t>(std::clamp(y0 + r_coeff, 0, 255)); // R
                *(rgba.data() + rgba_idx + 1) = static_cast<std::uint8_t>(std::clamp(y0 + g_coeff, 0, 255)); // G
                *(rgba.data() + rgba_idx + 2) = static_cast<std::uint8_t>(std::clamp(y0 + b_coeff, 0, 255)); // B
                *(rgba.data() + rgba_idx + 3) = 255;

                *(rgba.data() + rgba_idx + 4) = static_cast<std::uint8_t>(std::clamp(y1 + r_coeff, 0, 255)); // R
                *(rgba.data() + rgba_idx + 5) = static_cast<std::uint8_t>(std::clamp(y1 + g_coeff, 0, 255)); // G
                *(rgba.data() + rgba_idx + 6) = static_cast<std::uint8_t>(std::clamp(y1 + b_coeff, 0, 255)); // B
                *(rgba.data() + rgba_idx + 7) = 255;
            }
        }
    }

    bool CameraDriver::setImageToROSMessage(sensor_msgs::msg::Image::SharedPtr image_msg)
    {
        bool ret = false;

        struct v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if(xioctl(fd_, VIDIOC_DQBUF,  &buf) == -1){
            // ノンブロッキングモードでは `EAGAIN` が返された場合、現時点でバッファが利用できないことを意味する
            // 補足: この場合は再試行待ちのために無理にログを出さずに静かにリターンする
            // DQBUF に成功した場合のみ後続処理（変換→メッセージ生成→QBUF再キュー）を行う
            if(errno == EAGAIN){
                return ret;
            }
            RCLCPP_ERROR(logger_, "Error dequeueing buffer (errno=%d)", errno);
            return ret;
        }

        // 実体そのものを参照するため、コピーが発生しない
        const Buffer & buffer = buffers_[buf.index];

        std::vector<std::uint8_t> rgba_image_data(original_width_ * original_height_ * 4);
        
        // ユーザー空間にマッピングされた先のアドレスを取得
        const std::uint8_t * data_ptr = static_cast<const std::uint8_t *>(buffer.start);

        std::chrono::system_clock::time_point start, end;
        if(measure_latency_){
            start = std::chrono::system_clock::now();
        }
        // mmap されたバッファから直接変換（コピーを省く）
        yuyv422_to_rgba8(data_ptr, fmt_.fmt.pix.sizeimage, rgba_image_data);
        if(measure_latency_){
            end = std::chrono::system_clock::now();
            double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end-start).count();
            if(use_gpu_) RCLCPP_INFO(logger_, "GPU process time : %f microsec", elapsed);
            else if(use_opencv_) RCLCPP_INFO(logger_, "CPU(opencv) process time : %f microsec", elapsed);
            else RCLCPP_INFO(logger_, "CPU(without opencv) process time : %f microsec", elapsed);
        }


        // 生成したRGBAデータをコピーせずムーブしてROSメッセージに格納
        // 補足: ここでムーブすることで大きなバッファの二重コピーを避ける
        // 注意: この時点で `rgba_image_data` の中身は image_msg が所有するので、以降アクセスできない
        image_msg->data = std::move(rgba_image_data);

        image_msg->header.stamp = this->get_clock()->now();
        image_msg->header.frame_id = camera_frame_id_;
        image_msg->height = fmt_.fmt.pix.height;
        image_msg->width = fmt_.fmt.pix.width;
        image_msg->step = fmt_.fmt.pix.bytesperline;
        image_msg->encoding = sensor_msgs::image_encodings::RGBA8;

        // バッファを再キューしてドライバが再利用できるようにする
        // 補足: DQBUF で取り出したバッファはユーザ空間で処理したら必ず QBUF で返却する必要がある。
        // 返却しないとドライバ側の空きバッファが枯渇し、以降のフレーム取得が停止する。
        if(xioctl(fd_, VIDIOC_QBUF, &buf) == -1){
            RCLCPP_ERROR(logger_, "Error queueing buffer (errno=%d)", errno);
            // ROS メッセージ生成は成功と見なす
        }

        ret = true;
        return ret;
    }

    bool CameraDriver::publishImageMessage(sensor_msgs::msg::Image::SharedPtr image_msg)
    {
        bool ret = false;

        image_pub_->publish(*image_msg);

        ret = true;
        return ret;
    }

    bool CameraDriver::stop()
    {
        bool ret = false;

        enum v4l2_buf_type type;
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(xioctl(fd_, VIDIOC_STREAMOFF, &type) == -1){
            return ret;
        }

        // 確保したメモリを解放する
        free(buffers_);

        ret = true;
        return ret;
    }

} // namespace camera_driver