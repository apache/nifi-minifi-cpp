# Baseline capture — namespace `baseline-s2s`

## Entity counts

- `fs_path`: 27
- `kafka_topic`: 1
- `nifi_data`: 4
- `nifi_flow`: 1
- `nifi_flow_path`: 7
- `nifi_input_port`: 1
- `nifi_queue`: 1

## Flow paths

- `Remote Input Port` — qn `26d15138-47d8-3bff-aa67-e6bec15c2c20@baseline-s2s`
- `baseline-input, s2s-sink` — qn `67fceaac-01a0-1000-1f81-9894fb3cceac@baseline-s2s`
- `kafka-source, kafka-publish` — qn `67fceadf-01a0-1000-2988-867ea905b0c8@baseline-s2s`
- `kafka-consume, kafka-sink` — qn `67fceaee-01a0-1000-f73d-366da137bf99@baseline-s2s`
- `file-get, file-put` — qn `67fceb0e-01a0-1000-bbf8-6dfaa9f20ddb@baseline-s2s`
- `s3-source, s3-put` — qn `67fceb23-01a0-1000-d304-75f07b6b727b@baseline-s2s`
- `s2s-source` — qn `67fcebab-01a0-1000-91e6-72e2824e76e0@baseline-s2s`

## Datasets

### `fs_path`
- qn `/tmp/atlas-baseline/in/hello3.txt@baseline-s2s` name=`/tmp/atlas-baseline/in/hello3.txt`
- qn `/tmp/atlas-baseline/in/hello4.txt@baseline-s2s` name=`/tmp/atlas-baseline/in/hello4.txt`
- qn `/tmp/atlas-baseline/out/hello3.txt@baseline-s2s` name=`/tmp/atlas-baseline/out/hello3.txt`
- qn `/tmp/atlas-baseline/out/hello4.txt@baseline-s2s` name=`/tmp/atlas-baseline/out/hello4.txt`
- qn `/tmp/atlas-baseline/s2s-out/1095e247-ab31-4a64-9f9a-0224843b97d7@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/1095e247-ab31-4a64-9f9a-0224843b97d7`
- qn `/tmp/atlas-baseline/s2s-out/15c57eb4-8384-4d52-a659-28a683abc3d3@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/15c57eb4-8384-4d52-a659-28a683abc3d3`
- qn `/tmp/atlas-baseline/s2s-out/1b130a44-a73f-49cd-a458-d96a83a973db@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/1b130a44-a73f-49cd-a458-d96a83a973db`
- qn `/tmp/atlas-baseline/s2s-out/255df3a8-7b74-46e2-8946-96d042e9bb16@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/255df3a8-7b74-46e2-8946-96d042e9bb16`
- qn `/tmp/atlas-baseline/s2s-out/2c0bbc00-d4ab-4947-bbed-ad8dd48051f3@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/2c0bbc00-d4ab-4947-bbed-ad8dd48051f3`
- qn `/tmp/atlas-baseline/s2s-out/3facd579-285e-4bee-bd63-04fe0b1dfeb2@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/3facd579-285e-4bee-bd63-04fe0b1dfeb2`
- qn `/tmp/atlas-baseline/s2s-out/552162e5-09df-4624-80a2-3ceebc06da98@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/552162e5-09df-4624-80a2-3ceebc06da98`
- qn `/tmp/atlas-baseline/s2s-out/64236d68-5a8c-4289-b19b-ba2176fd38e4@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/64236d68-5a8c-4289-b19b-ba2176fd38e4`
- qn `/tmp/atlas-baseline/s2s-out/7650fae5-620c-4f24-9e7e-9e5917fe5589@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/7650fae5-620c-4f24-9e7e-9e5917fe5589`
- qn `/tmp/atlas-baseline/s2s-out/7e530d42-e647-4702-a89d-c8c396d8182a@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/7e530d42-e647-4702-a89d-c8c396d8182a`
- qn `/tmp/atlas-baseline/s2s-out/8317aa33-e6cf-4dc7-b215-d3d247106229@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/8317aa33-e6cf-4dc7-b215-d3d247106229`
- qn `/tmp/atlas-baseline/s2s-out/836dda10-a12d-4f51-893c-260ae5f1d03d@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/836dda10-a12d-4f51-893c-260ae5f1d03d`
- qn `/tmp/atlas-baseline/s2s-out/89e07a78-d063-42de-b6cf-228e2cc29c94@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/89e07a78-d063-42de-b6cf-228e2cc29c94`
- qn `/tmp/atlas-baseline/s2s-out/a86c8a7b-11b8-4e0a-80f1-8a013a7732c1@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/a86c8a7b-11b8-4e0a-80f1-8a013a7732c1`
- qn `/tmp/atlas-baseline/s2s-out/bba0b5b1-045c-4d50-81d3-af0279ee6bc3@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/bba0b5b1-045c-4d50-81d3-af0279ee6bc3`
- qn `/tmp/atlas-baseline/s2s-out/bd0fa7b2-5ffe-4c2b-b231-ecd96f56599a@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/bd0fa7b2-5ffe-4c2b-b231-ecd96f56599a`
- qn `/tmp/atlas-baseline/s2s-out/bfdae630-77f2-4503-94bd-0a5f020369e4@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/bfdae630-77f2-4503-94bd-0a5f020369e4`
- qn `/tmp/atlas-baseline/s2s-out/c66f38d5-1114-4541-aa5a-56c44f63c6f3@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/c66f38d5-1114-4541-aa5a-56c44f63c6f3`
- qn `/tmp/atlas-baseline/s2s-out/cabe5e69-fc4d-48df-aac7-27ed1346c290@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/cabe5e69-fc4d-48df-aac7-27ed1346c290`
- qn `/tmp/atlas-baseline/s2s-out/cc5009ab-38ad-4ba1-99ba-21c24bd2b56b@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/cc5009ab-38ad-4ba1-99ba-21c24bd2b56b`
- qn `/tmp/atlas-baseline/s2s-out/e7e7f4fd-d64d-4e77-a252-fdab393ad6b3@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/e7e7f4fd-d64d-4e77-a252-fdab393ad6b3`
- qn `/tmp/atlas-baseline/s2s-out/ec4809f4-4ed6-45ab-94c1-f261febfb5ed@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/ec4809f4-4ed6-45ab-94c1-f261febfb5ed`
- qn `/tmp/atlas-baseline/s2s-out/fa5ce3c1-f6a9-4ae6-b9a2-ac0b25ad9341@baseline-s2s` name=`/tmp/atlas-baseline/s2s-out/fa5ce3c1-f6a9-4ae6-b9a2-ac0b25ad9341`

### `kafka_topic`
- qn `baseline-kafka-topic@baseline-s2s` name=`baseline-kafka-topic`

### `nifi_data`
- qn `67fceadf-01a0-1000-2988-867ea905b0c8@baseline-s2s` name=`GenerateFlowFile`
- qn `67fceb23-01a0-1000-d304-75f07b6b727b@baseline-s2s` name=`GenerateFlowFile`
- qn `67fceb27-01a0-1000-b297-dd785e5ccf12@baseline-s2s` name=`PutS3Object`
- qn `67fcebab-01a0-1000-91e6-72e2824e76e0@baseline-s2s` name=`GenerateFlowFile`

### `nifi_input_port`
- qn `67fceaac-01a0-1000-1f81-9894fb3cceac@baseline-s2s` name=`baseline-input`

