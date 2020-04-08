# Magic Mask

A behavior-changing application to help stop the spread of viruses -- https://magicmask.stanford.edu/

## Build

First, install dependencies by following [this document](https://github.com/tse-group/magicmask/blob/master/mediapipe/docs/install.md) and then build the mediapipe backend

```bash
bazel build \                                   
        -c opt \
        --define MEDIAPIPE_DISABLE_GPU=1 \
        --define libunwind=true \
        mediapipe/examples/desktop/OWN_facehand_tracking:OWN_facehand_tracking --sandbox_debug --verbose_failures
```

Then build the frontend and package the binary.

```bash
cd frontend
go build frontend
bash package.sh
bash update.sh
```
