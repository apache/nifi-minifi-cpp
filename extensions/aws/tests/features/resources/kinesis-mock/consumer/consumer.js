import { KinesisClient, DescribeStreamCommand, GetShardIteratorCommand, GetRecordsCommand } from "@aws-sdk/client-kinesis";
import { NodeHttpHandler } from "@aws-sdk/node-http-handler";

const args = process.argv.slice(2);
if (args.length === 0) {
    console.error("Usage: node consumer.js <search-string>");
    process.exit(1);
}

const searchString = args[0];
const streamName = "test_stream";

const kinesis = new KinesisClient({
    endpoint: "http://localhost:4568",
    region: "us-east-1",
    credentials: { accessKeyId: "fake", secretAccessKey: "fake" },
    requestHandler: new NodeHttpHandler({ http2: false }),
});

async function getShardIterator(shardId) {
    const command = new GetShardIteratorCommand({
        StreamName: streamName,
        ShardId: shardId,
        ShardIteratorType: "TRIM_HORIZON",
    });
    const response = await kinesis.send(command);
    return response.ShardIterator;
}

async function readShardRecords(shardId) {
    let shardIterator = await getShardIterator(shardId);
    const getRecordsCommand = new GetRecordsCommand({ ShardIterator: shardIterator });
    const data = await kinesis.send(getRecordsCommand);

    return data.Records.map(r => Buffer.from(r.Data).toString().trim());
}

async function readOnce() {
    try {
        console.log(`Checking stream '${streamName}' for: "${searchString}"`);

        const describeCommand = new DescribeStreamCommand({ StreamName: streamName });
        const { StreamDescription } = await kinesis.send(describeCommand);

        for (let shard of StreamDescription.Shards) {
            console.log(`Reading from shard: ${shard.ShardId}`);

            const records = await readShardRecords(shard.ShardId);

            if (records.includes(searchString)) {
                console.log(`Found "${searchString}" in records.`);
                process.exit(0);
            }
        }

        console.log(`"${searchString}" not found in any shard.`);
        process.exit(-1);
    } catch (error) {
        console.error("Error reading stream:", error);
        process.exit(1);
    }
}

readOnce();
