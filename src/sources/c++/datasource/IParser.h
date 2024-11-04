#pragma once
#include "IDataSource.h"
namespace mdp {
    class IParser {
        public:
        IParser(const std::shared_ptr<IDataSource>& datasource);
        IParser(const std::string& path, Type type);
        virtual ~IParser();
        virtual int startParse() = 0;
        virtual void loadPackets(int size) = 0;
        virtual bool supportFormat() = 0;
        virtual std::string dumpFormats(int full = 0) = 0;
        protected:
        std::shared_ptr<IDataSource> _datasource;
    };
}
