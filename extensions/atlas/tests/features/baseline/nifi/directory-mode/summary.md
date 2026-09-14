# Baseline capture — namespace `baseline-dir`

## Entity counts

- `fs_path`: 2
- `kafka_topic`: 1
- `nifi_data`: 4
- `nifi_flow`: 1
- `nifi_flow_path`: 6
- `nifi_input_port`: 1

## Flow paths

- `baseline-input, s2s-sink` — qn `67cb24b1-01a0-1000-de74-970cd7c119a7@baseline-dir`
- `kafka-source, kafka-publish` — qn `67cb24fb-01a0-1000-25c0-9ad5b1913eff@baseline-dir`
- `kafka-consume, kafka-sink` — qn `67cb250e-01a0-1000-5fbd-3db37aa48503@baseline-dir`
- `file-get, file-put` — qn `67cb2536-01a0-1000-9400-6cea63132761@baseline-dir`
- `s3-source, s3-put` — qn `67cb254b-01a0-1000-b6c6-9105dd2cdfe3@baseline-dir`
- `s2s-source` — qn `67cb257e-01a0-1000-717d-89d3a514fb69@baseline-dir`

## Datasets

### `fs_path`
- qn `/tmp/atlas-baseline/in@baseline-dir` name=`/tmp/atlas-baseline/in`
- qn `/tmp/atlas-baseline/out@baseline-dir` name=`/tmp/atlas-baseline/out`

### `kafka_topic`
- qn `baseline-kafka-topic@baseline-dir` name=`baseline-kafka-topic`

### `nifi_data`
- qn `67cb24fb-01a0-1000-25c0-9ad5b1913eff@baseline-dir` name=`GenerateFlowFile`
- qn `67cb254b-01a0-1000-b6c6-9105dd2cdfe3@baseline-dir` name=`GenerateFlowFile`
- qn `67cb2562-01a0-1000-68c4-25a121e9f411@baseline-dir` name=`PutS3Object`
- qn `67cb257e-01a0-1000-717d-89d3a514fb69@baseline-dir` name=`GenerateFlowFile`

### `nifi_input_port`
- qn `67cb24b1-01a0-1000-de74-970cd7c119a7@baseline-dir` name=`baseline-input`

