/*
 * This file is part of libArcus
 *
 * Copyright (C) 2016 Ultimaker b.v. <a.hiemstra@ultimaker.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "MessageTypeStore.h"

#include <unordered_map>
#include <sstream>
#include <iostream>

#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/compiler/importer.h>

using namespace Arcus;

class ErrorCollector : public google::protobuf::compiler::MultiFileErrorCollector
{
public:
    void AddError(const std::string& filename, int line, int column, const std::string& message) override
    {
        _stream << "[" << filename << " (" << line << "," << column << ")] " << message << std::endl;
        _error_count++;
    }

    std::string getAllErrors()
    {
        return _stream.str();
    }

    int getErrorCount()
    {
        return _error_count;
    }

private:
    std::stringstream _stream;
    int _error_count = 0;
};

class MessageTypeStore::Private
{
public:
    std::unordered_map<uint, const google::protobuf::Message*> message_types;
    std::unordered_map<const google::protobuf::Descriptor*, uint> message_type_mapping;

    std::shared_ptr<ErrorCollector> error_collector;
    std::shared_ptr<google::protobuf::compiler::DiskSourceTree> source_tree;
    std::shared_ptr<google::protobuf::compiler::Importer> importer;
    std::shared_ptr<google::protobuf::DynamicMessageFactory> message_factory;
};

Arcus::MessageTypeStore::MessageTypeStore() : d(new Private)
{
}

Arcus::MessageTypeStore::~MessageTypeStore()
{
}

bool Arcus::MessageTypeStore::hasType(uint type_id) const
{
    if(d->message_types.find(type_id) != d->message_types.end())
    {
        return true;
    }

    return false;
}

bool Arcus::MessageTypeStore::hasType(const std::string& type_name) const
{
    uint type_id = std::hash<std::string>{}(type_name);
    return hasType(type_id);
}

MessagePtr Arcus::MessageTypeStore::createMessage(uint type_id) const
{
    if(!hasType(type_id))
    {
        return MessagePtr{};
    }

    return MessagePtr(d->message_types[type_id]->New());
}

MessagePtr Arcus::MessageTypeStore::createMessage(const std::string& type_name) const
{
    uint type_id = std::hash<std::string>{}(type_name);
    return createMessage(type_id);
}

uint Arcus::MessageTypeStore::getMessageTypeId(const MessagePtr& message)
{
    return std::hash<std::string>{}(message->GetTypeName());
}

bool Arcus::MessageTypeStore::registerMessageType(const google::protobuf::Message* message_type)
{
    uint type_id = std::hash<std::string>{}(message_type->GetTypeName());

    if(hasType(type_id))
    {
        return false;
    }

    d->message_types[type_id] = message_type;
    d->message_type_mapping[message_type->GetDescriptor()] = type_id;

    return true;
}

bool Arcus::MessageTypeStore::registerAllMessageTypes(const std::string& file_name)
{
    if(!d->importer)
    {
        d->error_collector = std::make_shared<ErrorCollector>();
        d->source_tree = std::make_shared<google::protobuf::compiler::DiskSourceTree>();
        d->source_tree->MapPath("/", "/");
        d->importer = std::make_shared<google::protobuf::compiler::Importer>(d->source_tree.get(), d->error_collector.get());
    }

    auto descriptor = d->importer->Import(file_name);
    if(d->error_collector->getErrorCount() > 0)
    {
        return false;
    }

    if(!d->message_factory)
    {
        d->message_factory = std::make_shared<google::protobuf::DynamicMessageFactory>();
    }

    for(int i = 0; i < descriptor->message_type_count(); ++i)
    {
        auto message_type_descriptor = descriptor->message_type(i);

        auto message_type = d->message_factory->GetPrototype(message_type_descriptor);

        uint type_id = std::hash<std::string>{}(message_type->GetTypeName());

        d->message_types[type_id] = message_type;
        d->message_type_mapping[message_type_descriptor] = type_id;
    }

    return true;
}

void Arcus::MessageTypeStore::dumpMessageTypes()
{
    for(auto type : d->message_types)
    {
        std::cout << "Type ID: " << type.first << " Type Name: " << type.second->GetTypeName() << std::endl;
    }
}