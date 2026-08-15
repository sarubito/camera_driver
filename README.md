# camera_driver

ROS2 用の Web カメラドライバ（Ubuntu 向け）

**動作確認環境**
- ROS 2: Jazzy
- OS: Ubuntu 24.04

**概要**
- V4L2 の MMAP モードを用い、低レイテンシでカメラフレームを取得して ROS2 の `sensor_msgs/Image` として配信します。
- ピクセルフォーマットは YUYV (YUV422) を想定し、RGBA8 に変換して配信します。
- 変換は GPU (CUDA) と CPU の両方に対応します。CPU 動作時は OpenCV を用いる経路と、OpenCV を使わない軽量な経路の切り替えが可能です。
- 動作確認には、Logitech, Inc. Webcam C270を使用。

** レイテンシ計測 **
- CPU処理(Intel CORE i7 (no opencv))
- CPU処理(Intel CORE i7 (opencv 4.6.0))
- GPU処理(NVIDIA GeForce RTX 3070 (cuda13.3))

**主な機能**
- MMAP ベースの V4L2 バッファ管理（`VIDIOC_REQBUFS` / `VIDIOC_QUERYBUF` / `mmap` / `VIDIOC_QBUF` / `VIDIOC_DQBUF`）
- ノンブロッキング動作（`O_NONBLOCK`）でタイマーやエグゼキュータを阻害しない設計
- YUYV -> RGBA8 の変換を CPU/CUDA 双方で実装
- 変換後データは std::move を用いて ROS メッセージへムーブ格納（余分なコピーを削減）

**ビルド**
1. ワークスペースルートでビルドします:

```bash
colcon build --symlink-install --packages-up-to camera_driver
source install/setup.bash
```

**実行**
- 直接ノードを起動する例:

```bash
ros2 run camera_driver main_node
```

- launch ファイルから起動する例（インストール済みの launch ディレクトリを使用）:

```bash
ros2 launch camera_driver camera_driver.launch.py
```

**ノードパラメータ（主なもの）**
- `use_gpu` : bool (default: `false`) — CUDA を使うか
- `use_opencv` : bool (default: `false`) — CPU 変換で OpenCV を使うか
- `device_file` : string (default: `/dev/video0`) — カメラデバイスファイル
- `camera_frame_id` : string (default: `camera`)
- `image_publish_hz` : int (default: `10`)
- `info_publish_hz` : int (default: `1`)
- `n_buffers` : int (default: `4`)
- `original_height` / `original_width` : int (default: `656` / `1184`)

**注意事項**
- `mmap()` により得られるバッファのアドレス（内部で保持している `buffer.start`）はカーネルが管理する領域をユーザー空間にマップしたポインタです。`VIDIOC_DQBUF` で取り出した後はユーザー側で安全に読み書きできますが、処理後は必ず `VIDIOC_QBUF` で再キューして下さい。再キュー後にそのポインタへアクセスするとデータ競合や破損が発生します。
- GPU（CUDA）経路で mmap されたホストメモリを直接デバイスに転送する場合、ページングの影響で性能が出ないことがあります。高スループットが必要な場合は `cudaHostRegister` によるピン留めや、別途ピン留め済みバッファへのコピーを検討してください。

**ライセンス**
- リポジトリルートの `LICENSE` を参照してください。

---

この README は簡易ガイドです。詳細なパラメータや起動オプションはソース内のドキュメントと `param/camera_driver_params.yaml` を参照してください。
