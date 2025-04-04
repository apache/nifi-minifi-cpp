FROM node:lts-alpine
WORKDIR /app
RUN npm i kinesis-local
RUN npm i @aws-sdk/client-kinesis @aws-sdk/node-http-handler
COPY server.json ./
COPY consumer ./consumer

ENTRYPOINT npx kinesis-local
