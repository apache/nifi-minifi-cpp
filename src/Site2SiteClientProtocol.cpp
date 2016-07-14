/**
 * @file Site2SiteProtocol.cpp
 * Site2SiteProtocol class implementation
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <sys/time.h>
#include <stdio.h>
#include <time.h>
#include <chrono>
#include <thread>
#include <random>
#include <netinet/tcp.h>
#include <iostream>
#include "Site2SitePeer.h"
#include "Site2SiteClientProtocol.h"

bool Site2SiteClientProtocol::establish()
{
	if (_peerState != IDLE)
	{
		_logger->log_error("Site2Site peer state is not idle while try to establish");
		return false;
	}

	bool ret = _peer->Open();

	if (!ret)
	{
		_logger->log_error("Site2Site peer socket open failed");
		return false;
	}

	// Negotiate the version
	ret = initiateResourceNegotiation();

	if (!ret)
	{
		_logger->log_error("Site2Site Protocol Version Negotiation failed");
		/*
		_peer->yield();
		tearDown(); */
		return false;
	}

	_logger->log_info("Site2Site socket established");
	_peerState = ESTABLISHED;

	return true;
}

bool Site2SiteClientProtocol::initiateResourceNegotiation()
{
	// Negotiate the version
	if (_peerState != IDLE)
	{
		_logger->log_error("Site2Site peer state is not idle while initiateResourceNegotiation");
		return false;
	}

	_logger->log_info("Negotiate protocol version with destination port %s current version %d", _portIdStr.c_str(), _currentVersion);

	int ret = _peer->writeUTF(this->getResourceName());

	if (ret <= 0)
	{
		// tearDown();
		return false;
	}

	ret = _peer->write(_currentVersion);

	if (ret <= 0)
	{
		// tearDown();
		return false;
	}

	uint8_t statusCode;
	ret = _peer->read(statusCode);

	if (ret <= 0)
	{
		// tearDown();
		return false;
	}

	switch (statusCode)
	{
	case RESOURCE_OK:
		_logger->log_info("Site2Site Protocol Negotiate protocol version OK");
		return true;
	case DIFFERENT_RESOURCE_VERSION:
		uint32_t serverVersion;
		ret = _peer->read(serverVersion);
		if (ret <= 0)
		{
			// tearDown();
			return false;
		}
		_logger->log_info("Site2Site Server Response asked for a different protocol version %d", serverVersion);
		for (unsigned int i = (_currentVersionIndex + 1); i < sizeof(_supportedVersion)/sizeof(uint32_t); i++)
		{
			if (serverVersion >= _supportedVersion[i])
			{
				_currentVersion = _supportedVersion[i];
				_currentVersionIndex = i;
				return initiateResourceNegotiation();
			}
		}
		ret = -1;
		// tearDown();
		return false;
	case NEGOTIATED_ABORT:
		_logger->log_info("Site2Site Negotiate protocol response ABORT");
		ret = -1;
		// tearDown();
		return false;
	default:
		_logger->log_info("Negotiate protocol response unknown code %d", statusCode);
		return true;
	}

	return true;
}

bool Site2SiteClientProtocol::initiateCodecResourceNegotiation()
{
	// Negotiate the version
	if (_peerState != HANDSHAKED)
	{
		_logger->log_error("Site2Site peer state is not handshaked while initiateCodecResourceNegotiation");
		return false;
	}

	_logger->log_info("Negotiate Codec version with destination port %s current version %d", _portIdStr.c_str(), _currentCodecVersion);

	int ret = _peer->writeUTF(this->getCodecResourceName());

	if (ret <= 0)
	{
		// tearDown();
		return false;
	}

	ret = _peer->write(_currentCodecVersion);

	if (ret <= 0)
	{
		// tearDown();
		return false;
	}

	uint8_t statusCode;
	ret = _peer->read(statusCode);

	if (ret <= 0)
	{
		// tearDown();
		return false;
	}

	switch (statusCode)
	{
	case RESOURCE_OK:
		_logger->log_info("Site2Site Codec Negotiate version OK");
		return true;
	case DIFFERENT_RESOURCE_VERSION:
		uint32_t serverVersion;
		ret = _peer->read(serverVersion);
		if (ret <= 0)
		{
			// tearDown();
			return false;
		}
		_logger->log_info("Site2Site Server Response asked for a different codec version %d", serverVersion);
		for (unsigned int i = (_currentCodecVersionIndex + 1); i < sizeof(_supportedCodecVersion)/sizeof(uint32_t); i++)
		{
			if (serverVersion >= _supportedCodecVersion[i])
			{
				_currentCodecVersion = _supportedCodecVersion[i];
				_currentCodecVersionIndex = i;
				return initiateCodecResourceNegotiation();
			}
		}
		ret = -1;
		// tearDown();
		return false;
	case NEGOTIATED_ABORT:
		_logger->log_info("Site2Site Codec Negotiate response ABORT");
		ret = -1;
		// tearDown();
		return false;
	default:
		_logger->log_info("Negotiate Codec response unknown code %d", statusCode);
		return true;
	}

	return true;
}

bool Site2SiteClientProtocol::handShake()
{
	if (_peerState != ESTABLISHED)
	{
		_logger->log_error("Site2Site peer state is not established while handshake");
		return false;
	}
	_logger->log_info("Site2Site Protocol Perform hand shake with destination port %s", _portIdStr.c_str());
	uuid_t uuid;
	// Generate the global UUID for the com identify
	uuid_generate(uuid);
	char uuidStr[37];
	uuid_unparse(uuid, uuidStr);
	_commsIdentifier = uuidStr;

	int ret = _peer->writeUTF(_commsIdentifier);

	if (ret <= 0)
	{
		// tearDown();
		return false;
	}

	std::map<std::string, std::string> properties;
	properties[HandShakePropertyStr[GZIP]] = "false";
	properties[HandShakePropertyStr[PORT_IDENTIFIER]] = _portIdStr;
	properties[HandShakePropertyStr[REQUEST_EXPIRATION_MILLIS]] = std::to_string(this->_timeOut);
	if (this->_currentVersion >= 5)
	{
		if (this->_batchCount > 0)
			properties[HandShakePropertyStr[BATCH_COUNT]] = std::to_string(this->_batchCount);
		if (this->_batchSize > 0)
			properties[HandShakePropertyStr[BATCH_SIZE]] = std::to_string(this->_batchSize);
		if (this->_batchDuration > 0)
			properties[HandShakePropertyStr[BATCH_DURATION]] = std::to_string(this->_batchDuration);
	}

	if (_currentVersion >= 3)
	{
		ret = _peer->writeUTF(_peer->getURL());
		if (ret <= 0)
		{
			// tearDown();
			return false;
		}
	}

	uint32_t size = properties.size();
	ret = _peer->write(size);
	if (ret <= 0)
	{
		// tearDown();
		return false;
	}

	std::map<std::string, std::string>::iterator it;
	for (it = properties.begin(); it!= properties.end(); it++)
	{
		ret = _peer->writeUTF(it->first);
		if (ret <= 0)
		{
			// tearDown();
			return false;
		}
		ret = _peer->writeUTF(it->second);
		if (ret <= 0)
		{
			// tearDown();
			return false;
		}
		_logger->log_info("Site2Site Protocol Send handshake properties %s %s", it->first.c_str(), it->second.c_str());
	}

	RespondCode code;
	std::string message;

	ret = this->readRespond(code, message);

	if (ret <= 0)
	{
		// tearDown();
		return false;
	}

	switch (code)
	{
	case PROPERTIES_OK:
		_logger->log_info("Site2Site HandShake Completed");
		_peerState = HANDSHAKED;
		return true;
	case PORT_NOT_IN_VALID_STATE:
    case UNKNOWN_PORT:
    case PORTS_DESTINATION_FULL:
    	_logger->log_error("Site2Site HandShake Failed because destination port is either invalid or full");
		ret = -1;
		/*
		_peer->yield();
		tearDown(); */
		return false;
	default:
		_logger->log_info("HandShake Failed because of unknown respond code %d", code);
		ret = -1;
		/*
		_peer->yield();
		tearDown(); */
		return false;
	}

	return false;
}

void Site2SiteClientProtocol::tearDown()
{
	if (_peerState >= ESTABLISHED)
	{
		_logger->log_info("Site2Site Protocol tearDown");
		// need to write shutdown request
		writeRequestType(SHUTDOWN);
	}

	std::map<std::string, Transaction *>::iterator it;
	for (it = _transactionMap.begin(); it!= _transactionMap.end(); it++)
	{
		delete it->second;
	}
	_transactionMap.clear();
	_peer->Close();
	_peerState = IDLE;
}

int Site2SiteClientProtocol::writeRequestType(RequestType type)
{
	if (type >= MAX_REQUEST_TYPE)
		return -1;

	return _peer->writeUTF(RequestTypeStr[type]);
}

int Site2SiteClientProtocol::readRequestType(RequestType &type)
{
	std::string requestTypeStr;

	int ret = _peer->readUTF(requestTypeStr);

	if (ret <= 0)
		return ret;

	for (int i = (int) NEGOTIATE_FLOWFILE_CODEC; i <= (int) SHUTDOWN; i++)
	{
		if (RequestTypeStr[i] == requestTypeStr)
		{
			type = (RequestType) i;
			return ret;
		}
	}

	return -1;
}

int Site2SiteClientProtocol::readRespond(RespondCode &code, std::string &message)
{
	uint8_t firstByte;

	int ret = _peer->read(firstByte);

	if (ret <= 0 || firstByte != CODE_SEQUENCE_VALUE_1)
		return -1;

	uint8_t secondByte;

	ret = _peer->read(secondByte);

	if (ret <= 0 || secondByte != CODE_SEQUENCE_VALUE_2)
		return -1;

	uint8_t thirdByte;

	ret = _peer->read(thirdByte);

	if (ret <= 0)
		return ret;

	code = (RespondCode) thirdByte;

	RespondCodeContext *resCode = this->getRespondCodeContext(code);

	if ( resCode == NULL)
	{
		// Not a valid respond code
		return -1;
	}
	if (resCode->hasDescription)
	{
		ret = _peer->readUTF(message);
		if (ret <= 0)
			return -1;
	}
	return 3 + message.size();
}

int Site2SiteClientProtocol::writeRespond(RespondCode code, std::string message)
{
	RespondCodeContext *resCode = this->getRespondCodeContext(code);

	if (resCode == NULL)
	{
		// Not a valid respond code
		return -1;
	}

	uint8_t codeSeq[3];
	codeSeq[0] = CODE_SEQUENCE_VALUE_1;
	codeSeq[1] = CODE_SEQUENCE_VALUE_2;
	codeSeq[2] = (uint8_t) code;

	int ret = _peer->write(codeSeq, 3);

	if (ret != 3)
		return -1;

	if (resCode->hasDescription)
	{
		ret = _peer->writeUTF(message);
		if (ret > 0)
			return (3 + ret);
		else
			return ret;
	}
	else
		return 3;
}

bool Site2SiteClientProtocol::negotiateCodec()
{
	if (_peerState != HANDSHAKED)
	{
		_logger->log_error("Site2Site peer state is not handshaked while negotiate codec");
		return false;
	}

	_logger->log_info("Site2Site Protocol Negotiate Codec with destination port %s", _portIdStr.c_str());

	int status = this->writeRequestType(NEGOTIATE_FLOWFILE_CODEC);

	if (status <= 0)
	{
		// tearDown();
		return false;
	}

	// Negotiate the codec version
	bool ret = initiateCodecResourceNegotiation();

	if (!ret)
	{
		_logger->log_error("Site2Site Codec Version Negotiation failed");
		/*
		_peer->yield();
		tearDown(); */
		return false;
	}

	_logger->log_info("Site2Site Codec Completed and move to READY state for data transfer");
	_peerState = READY;

	return true;
}

bool Site2SiteClientProtocol::bootstrap()
{
	if (_peerState == READY)
		return true;

	tearDown();

	if (establish() && handShake() && negotiateCodec())
	{
		_logger->log_info("Site2Site Ready For data transaction");
		return true;
	}
	else
	{
		_peer->yield();
		tearDown();
		return false;
	}
}

Transaction* Site2SiteClientProtocol::createTransaction(std::string &transactionID, TransferDirection direction)
{
	int ret;
	bool dataAvailable;
	Transaction *transaction = NULL;

	if (_peerState != READY)
	{
		bootstrap();
	}

	if (_peerState != READY)
	{
		return NULL;
	}

	if (direction == RECEIVE)
	{
		ret = writeRequestType(RECEIVE_FLOWFILES);

		if (ret <= 0)
		{
			// tearDown();
			return NULL;
		}

		RespondCode code;
		std::string message;

		ret = readRespond(code, message);

		if (ret <= 0)
		{
			// tearDown();
			return NULL;
		}

		switch (code)
		{
		case MORE_DATA:
			dataAvailable = true;
			_logger->log_info("Site2Site peer indicates that data is available");
			transaction = new Transaction(direction);
			_transactionMap[transaction->getUUIDStr()] = transaction;
			transactionID = transaction->getUUIDStr();
			transaction->setDataAvailable(dataAvailable);
			_logger->log_info("Site2Site create transaction %s", transaction->getUUIDStr().c_str());
			return transaction;
		case NO_MORE_DATA:
			dataAvailable = false;
			_logger->log_info("Site2Site peer indicates that no data is available");
			transaction = new Transaction(direction);
			_transactionMap[transaction->getUUIDStr()] = transaction;
			transactionID = transaction->getUUIDStr();
			transaction->setDataAvailable(dataAvailable);
			_logger->log_info("Site2Site create transaction %s", transaction->getUUIDStr().c_str());
			return transaction;
		default:
			_logger->log_info("Site2Site got unexpected response %d when asking for data", code);
			// tearDown();
			return NULL;
		}
	}
	else
	{
		ret = writeRequestType(SEND_FLOWFILES);

		if (ret <= 0)
		{
			// tearDown();
			return NULL;
		}
		else
		{
			transaction = new Transaction(direction);
			_transactionMap[transaction->getUUIDStr()] = transaction;
			transactionID = transaction->getUUIDStr();
			_logger->log_info("Site2Site create transaction %s", transaction->getUUIDStr().c_str());
			return transaction;
		}
	}
}

bool Site2SiteClientProtocol::receive(std::string transactionID, DataPacket *packet, bool &eof)
{
	int ret;
	Transaction *transaction = NULL;

	if (_peerState != READY)
	{
		bootstrap();
	}

	if (_peerState != READY)
	{
		return false;
	}

	std::map<std::string, Transaction *>::iterator it = this->_transactionMap.find(transactionID);

	if (it == _transactionMap.end())
	{
		return false;
	}
	else
	{
		transaction = it->second;
	}

	if (transaction->getState() != TRANSACTION_STARTED && transaction->getState() != DATA_EXCHANGED)
	{
		_logger->log_info("Site2Site transaction %s is not at started or exchanged state", transactionID.c_str());
		return false;
	}

	if (transaction->getDirection() != RECEIVE)
	{
		_logger->log_info("Site2Site transaction %s direction is wrong", transactionID.c_str());
		return false;
	}

	if (!transaction->isDataAvailable())
	{
		eof = true;
		return true;
	}

	if (transaction->_transfers > 0)
	{
		// if we already has transfer before, check to see whether another one is available
		RespondCode code;
		std::string message;

		ret = readRespond(code, message);

		if (ret <= 0)
		{
			return false;
		}
		if (code == CONTINUE_TRANSACTION)
		{
			_logger->log_info("Site2Site transaction %s peer indicate continue transaction", transactionID.c_str());
			transaction->_dataAvailable = true;
		}
		else if (code == FINISH_TRANSACTION)
		{
			_logger->log_info("Site2Site transaction %s peer indicate finish transaction", transactionID.c_str());
			transaction->_dataAvailable = false;
		}
		else
		{
			_logger->log_info("Site2Site transaction %s peer indicate wrong respond code %d", transactionID.c_str(), code);
			return false;
		}
	}

	if (!transaction->isDataAvailable())
	{
		eof = true;
		return true;
	}

	// start to read the packet
	uint32_t numAttributes;
	ret = _peer->read(numAttributes, &transaction->_crc);
	if (ret <= 0 || numAttributes > MAX_NUM_ATTRIBUTES)
	{
		return false;
	}

	// read the attributes
	for (unsigned int i = 0; i < numAttributes; i++)
	{
		std::string key;
		std::string value;
		ret = _peer->readUTF(key, true, &transaction->_crc);
		if (ret <= 0)
		{
			return false;
		}
		ret = _peer->readUTF(value, true, &transaction->_crc);
		if (ret <= 0)
		{
			return false;
		}
		packet->_attributes[key] = value;
		_logger->log_info("Site2Site transaction %s receives attribute key %s value %s", transactionID.c_str(), key.c_str(), value.c_str());
	}

	uint64_t len;
	ret = _peer->read(len, &transaction->_crc);
	if (ret <= 0)
	{
		return false;
	}

	packet->_size = len;
	transaction->_transfers++;
	transaction->_state = DATA_EXCHANGED;
	transaction->_bytes += len;
	_logger->log_info("Site2Site transaction %s receives flow record %d, total length %d", transactionID.c_str(),
			transaction->_transfers, transaction->_bytes);

	return true;
}

bool Site2SiteClientProtocol::send(std::string transactionID, DataPacket *packet, FlowFileRecord *flowFile, ProcessSession *session)
{
	int ret;
	Transaction *transaction = NULL;

	if (_peerState != READY)
	{
		bootstrap();
	}

	if (_peerState != READY)
	{
		return false;
	}

	std::map<std::string, Transaction *>::iterator it = this->_transactionMap.find(transactionID);

	if (it == _transactionMap.end())
	{
		return false;
	}
	else
	{
		transaction = it->second;
	}

	if (transaction->getState() != TRANSACTION_STARTED && transaction->getState() != DATA_EXCHANGED)
	{
		_logger->log_info("Site2Site transaction %s is not at started or exchanged state", transactionID.c_str());
		return false;
	}

	if (transaction->getDirection() != SEND)
	{
		_logger->log_info("Site2Site transaction %s direction is wrong", transactionID.c_str());
		return false;
	}

	if (transaction->_transfers > 0)
	{
		ret = writeRespond(CONTINUE_TRANSACTION, "CONTINUE_TRANSACTION");
		if (ret <= 0)
		{
			return false;
		}
	}

	// start to read the packet
	uint32_t numAttributes = packet->_attributes.size();
	ret = _peer->write(numAttributes, &transaction->_crc);
	if (ret != 4)
	{
		return false;
	}

	std::map<std::string, std::string>::iterator itAttribute;
	for (itAttribute = packet->_attributes.begin(); itAttribute!= packet->_attributes.end(); itAttribute++)
	{
		ret = _peer->writeUTF(itAttribute->first, true, &transaction->_crc);
		if (ret <= 0)
		{
			return false;
		}
		ret = _peer->writeUTF(itAttribute->second, true, &transaction->_crc);
		if (ret <= 0)
		{
			return false;
		}
		_logger->log_info("Site2Site transaction %s send attribute key %s value %s", transactionID.c_str(),
				itAttribute->first.c_str(), itAttribute->second.c_str());
	}

	uint64_t len = flowFile->getSize() ;
	ret = _peer->write(len, &transaction->_crc);
	if (ret != 8)
	{
		return false;
	}

	if (flowFile->getSize())
	{
		Site2SiteClientProtocol::ReadCallback callback(packet);
		session->read(flowFile, &callback);
		if (flowFile->getSize() != packet->_size)
		{
			return false;
		}
	}

	transaction->_transfers++;
	transaction->_state = DATA_EXCHANGED;
	transaction->_bytes += len;
	_logger->log_info("Site2Site transaction %s send flow record %d, total length %d", transactionID.c_str(),
				transaction->_transfers, transaction->_bytes);

	return true;
}

void Site2SiteClientProtocol::receiveFlowFiles(ProcessContext *context, ProcessSession *session)
{
	uint64_t bytes = 0;
	int transfers = 0;
	Transaction *transaction = NULL;

	if (_peerState != READY)
	{
		bootstrap();
	}

	if (_peerState != READY)
	{
		context->yield();
		tearDown();
		throw Exception(SITE2SITE_EXCEPTION, "Can not establish handshake with peer");
		return;
	}

	// Create the transaction
	std::string transactionID;
	transaction = createTransaction(transactionID, RECEIVE);

	if (transaction == NULL)
	{
		context->yield();
		tearDown();
		throw Exception(SITE2SITE_EXCEPTION, "Can not create transaction");
		return;
	}

	try
	{
		while (true)
		{
			std::map<std::string, std::string> empty;
			DataPacket packet(this, transaction, empty);
			bool eof = false;

			if (!receive(transactionID, &packet, eof))
			{
				throw Exception(SITE2SITE_EXCEPTION, "Receive Failed");
				return;
			}
			if (eof)
			{
				// transaction done
				break;
			}
			FlowFileRecord *flowFile = session->create();
			if (!flowFile)
			{
				throw Exception(SITE2SITE_EXCEPTION, "Flow File Creation Failed");
				return;
			}
			std::map<std::string, std::string>::iterator it;
			for (it = packet._attributes.begin(); it!= packet._attributes.end(); it++)
			{
				flowFile->addAttribute(it->first, it->second);
			}

			if (packet._size > 0)
			{
				Site2SiteClientProtocol::WriteCallback callback(&packet);
				session->write(flowFile, &callback);
				if (flowFile->getSize() != packet._size)
				{
					throw Exception(SITE2SITE_EXCEPTION, "Receive Size Not Right");
					return;
				}
			}
			Relationship relation; // undefined relationship
			session->transfer(flowFile, relation);
			// receive the transfer for the flow record
			bytes += packet._size;
			transfers++;
		} // while true

		if (!confirm(transactionID))
		{
			throw Exception(SITE2SITE_EXCEPTION, "Confirm Transaction Failed");
			return;
		}
		if (!complete(transactionID))
		{
			throw Exception(SITE2SITE_EXCEPTION, "Complete Transaction Failed");
			return;
		}
		_logger->log_info("Site2Site transaction %s successfully receive flow record %d, content bytes %d",
				transactionID.c_str(), transfers, bytes);
		// we yield the receive if we did not get anything
		if (transfers == 0)
			context->yield();
	}
	catch (std::exception &exception)
	{
		if (transaction)
			deleteTransaction(transactionID);
		context->yield();
		tearDown();
		_logger->log_debug("Caught Exception %s", exception.what());
		throw;
	}
	catch (...)
	{
		if (transaction)
			deleteTransaction(transactionID);
		context->yield();
		tearDown();
		_logger->log_debug("Caught Exception during Site2SiteClientProtocol::receiveFlowFiles");
		throw;
	}

	deleteTransaction(transactionID);

	return;
}

bool Site2SiteClientProtocol::confirm(std::string transactionID)
{
	int ret;
	Transaction *transaction = NULL;

	if (_peerState != READY)
	{
		bootstrap();
	}

	if (_peerState != READY)
	{
		return false;
	}

	std::map<std::string, Transaction *>::iterator it = this->_transactionMap.find(transactionID);

	if (it == _transactionMap.end())
	{
		return false;
	}
	else
	{
		transaction = it->second;
	}

	if (transaction->getState() == TRANSACTION_STARTED && !transaction->isDataAvailable() &&
			transaction->getDirection() == RECEIVE)
	{
		transaction->_state = TRANSACTION_CONFIRMED;
		return true;
	}

	if (transaction->getState() != DATA_EXCHANGED)
		return false;

	if (transaction->getDirection() == RECEIVE)
	{
		if (transaction->isDataAvailable())
			return false;
		// we received a FINISH_TRANSACTION indicator. Send back a CONFIRM_TRANSACTION message
		// to peer so that we can verify that the connection is still open. This is a two-phase commit,
		// which helps to prevent the chances of data duplication. Without doing this, we may commit the
		// session and then when we send the response back to the peer, the peer may have timed out and may not
		// be listening. As a result, it will re-send the data. By doing this two-phase commit, we narrow the
		// Critical Section involved in this transaction so that rather than the Critical Section being the
		// time window involved in the entire transaction, it is reduced to a simple round-trip conversation.
		long crcValue = transaction->getCRC();
		std::string crc = std::to_string(crcValue);
		_logger->log_info("Site2Site Send confirm with CRC %d to transaction %s", transaction->getCRC(),
						transactionID.c_str());
		ret = writeRespond(CONFIRM_TRANSACTION, crc);
		if (ret <= 0)
			return false;
		RespondCode code;
		std::string message;
		readRespond(code, message);
		if (ret <= 0)
			return false;

		if (code == CONFIRM_TRANSACTION)
		{
			_logger->log_info("Site2Site transaction %s peer confirm transaction", transactionID.c_str());
			transaction->_state = TRANSACTION_CONFIRMED;
			return true;
		}
		else if (code == BAD_CHECKSUM)
		{
			_logger->log_info("Site2Site transaction %s peer indicate bad checksum", transactionID.c_str());
			/*
			transaction->_state = TRANSACTION_CONFIRMED;
			return true; */
			return false;
		}
		else
		{
			_logger->log_info("Site2Site transaction %s peer unknown respond code %d",
					transactionID.c_str(), code);
			return false;
		}
	}
	else
	{
		_logger->log_info("Site2Site Send FINISH TRANSACTION for transaction %s",
								transactionID.c_str());
		ret = writeRespond(FINISH_TRANSACTION, "FINISH_TRANSACTION");
		if (ret <= 0)
			return false;
		RespondCode code;
		std::string message;
		readRespond(code, message);
		if (ret <= 0)
			return false;

		// we've sent a FINISH_TRANSACTION. Now we'll wait for the peer to send a 'Confirm Transaction' response
		if (code == CONFIRM_TRANSACTION)
		{
			_logger->log_info("Site2Site transaction %s peer confirm transaction with CRC %s", transactionID.c_str(), message.c_str());
			if (this->_currentVersion > 3)
			{
				long crcValue = transaction->getCRC();
				std::string crc = std::to_string(crcValue);
				if (message == crc)
				{
					_logger->log_info("Site2Site transaction %s CRC matched", transactionID.c_str());
					ret = writeRespond(CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION");
					if (ret <= 0)
						return false;
					transaction->_state = TRANSACTION_CONFIRMED;
					return true;
				}
				else
				{
					_logger->log_info("Site2Site transaction %s CRC not matched %s", transactionID.c_str(), crc.c_str());
					ret = writeRespond(BAD_CHECKSUM, "BAD_CHECKSUM");
					/*
					ret = writeRespond(CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION");
										if (ret <= 0)
											return false;
										transaction->_state = TRANSACTION_CONFIRMED;
					return true; */
					return false;
				}
			}
			ret = writeRespond(CONFIRM_TRANSACTION, "CONFIRM_TRANSACTION");
			if (ret <= 0)
				return false;
			transaction->_state = TRANSACTION_CONFIRMED;
			return true;
		}
		else
		{
			_logger->log_info("Site2Site transaction %s peer unknown respond code %d",
					transactionID.c_str(), code);
			return false;
		}
		return false;
	}
}

void Site2SiteClientProtocol::cancel(std::string transactionID)
{
	Transaction *transaction = NULL;

	if (_peerState != READY)
	{
		return;
	}

	std::map<std::string, Transaction *>::iterator it = this->_transactionMap.find(transactionID);

	if (it == _transactionMap.end())
	{
		return;
	}
	else
	{
		transaction = it->second;
	}

	if (transaction->getState() == TRANSACTION_CANCELED || transaction->getState() == TRANSACTION_COMPLETED
			|| transaction->getState() == TRANSACTION_ERROR)
	{
		return;
	}

	this->writeRespond(CANCEL_TRANSACTION, "Cancel");
	transaction->_state = TRANSACTION_CANCELED;

	tearDown();
	return;
}

void Site2SiteClientProtocol::deleteTransaction(std::string transactionID)
{
	Transaction *transaction = NULL;

	std::map<std::string, Transaction *>::iterator it = this->_transactionMap.find(transactionID);

	if (it == _transactionMap.end())
	{
		return;
	}
	else
	{
		transaction = it->second;
	}

	_logger->log_info("Site2Site delete transaction %s", transaction->getUUIDStr().c_str());
	delete transaction;
	_transactionMap.erase(transactionID);
}

void Site2SiteClientProtocol::error(std::string transactionID)
{
	Transaction *transaction = NULL;

	std::map<std::string, Transaction *>::iterator it = this->_transactionMap.find(transactionID);

	if (it == _transactionMap.end())
	{
		return;
	}
	else
	{
		transaction = it->second;
	}

	transaction->_state = TRANSACTION_ERROR;
	tearDown();
	return;
}

//! Complete the transaction
bool Site2SiteClientProtocol::complete(std::string transactionID)
{
	int ret;
	Transaction *transaction = NULL;

	if (_peerState != READY)
	{
		bootstrap();
	}

	if (_peerState != READY)
	{
		return false;
	}

	std::map<std::string, Transaction *>::iterator it = this->_transactionMap.find(transactionID);

	if (it == _transactionMap.end())
	{
		return false;
	}
	else
	{
		transaction = it->second;
	}

	if (transaction->getState() != TRANSACTION_CONFIRMED)
	{
		return false;
	}

	if (transaction->getDirection() == RECEIVE)
	{
		if (transaction->_transfers == 0)
		{
			transaction->_state = TRANSACTION_COMPLETED;
			return true;
		}
		else
		{
			_logger->log_info("Site2Site transaction %s send finished", transactionID.c_str());
			ret = this->writeRespond(TRANSACTION_FINISHED, "Finished");
			if (ret <= 0)
				return false;
			else
			{
				transaction->_state = TRANSACTION_COMPLETED;
				return true;
			}
		}
	}
	else
	{
		RespondCode code;
		std::string message;
		int ret;

		ret = readRespond(code, message);

		if (ret <= 0)
			return false;

		if (code == TRANSACTION_FINISHED)
		{
			_logger->log_info("Site2Site transaction %s peer finished transaction", transactionID.c_str());
			transaction->_state = TRANSACTION_COMPLETED;
			return true;
		}
		else
		{
			_logger->log_info("Site2Site transaction %s peer unknown respond code %d",
					transactionID.c_str(), code);
			return false;
		}
	}
}

void Site2SiteClientProtocol::transferFlowFiles(ProcessContext *context, ProcessSession *session)
{
	FlowFileRecord *flow = session->get();
	Transaction *transaction = NULL;

	if (!flow)
		return;

	if (_peerState != READY)
	{
		bootstrap();
	}

	if (_peerState != READY)
	{
		context->yield();
		tearDown();
		throw Exception(SITE2SITE_EXCEPTION, "Can not establish handshake with peer");
		return;
	}

	// Create the transaction
	std::string transactionID;
	transaction = createTransaction(transactionID, SEND);

	if (transaction == NULL)
	{
		context->yield();
		tearDown();
		throw Exception(SITE2SITE_EXCEPTION, "Can not create transaction");
		return;
	}

	bool continueTransaction = true;
	uint64_t startSendingNanos = getTimeNano();

	try
	{
		while (continueTransaction)
		{
			DataPacket packet(this, transaction, flow->getAttributes());

			if (!send(transactionID, &packet, flow, session))
			{
				throw Exception(SITE2SITE_EXCEPTION, "Send Failed");
				return;
			}
			_logger->log_info("Site2Site transaction %s send flow record %s",
							transactionID.c_str(), flow->getUUIDStr().c_str());
			session->remove(flow);

			uint64_t transferNanos = getTimeNano() - startSendingNanos;
			if (transferNanos > _batchSendNanos)
				break;

			flow = session->get();
			if (!flow)
			{
				continueTransaction = false;
			}
		} // while true

		if (!confirm(transactionID))
		{
			throw Exception(SITE2SITE_EXCEPTION, "Confirm Failed");
			return;
		}
		if (!complete(transactionID))
		{
			throw Exception(SITE2SITE_EXCEPTION, "Complete Failed");
			return;
		}
		_logger->log_info("Site2Site transaction %s successfully send flow record %d, content bytes %d",
				transactionID.c_str(), transaction->_transfers, transaction->_bytes);
	}
	catch (std::exception &exception)
	{
		if (transaction)
			deleteTransaction(transactionID);
		context->yield();
		tearDown();
		_logger->log_debug("Caught Exception %s", exception.what());
		throw;
	}
	catch (...)
	{
		if (transaction)
			deleteTransaction(transactionID);
		context->yield();
		tearDown();
		_logger->log_debug("Caught Exception during Site2SiteClientProtocol::transferFlowFiles");
		throw;
	}

	deleteTransaction(transactionID);

	return;
}
