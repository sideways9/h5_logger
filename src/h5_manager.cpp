#include <h5_manager.h>
#include <H5Cpp.h>

namespace Logger {
    H5Manager::H5Manager() : 
        h5file(nullptr)
    {
    }

    H5Manager::~H5Manager() {
        close();
    }

    bool H5Manager::open(const char * filename) {
        if(h5file != nullptr) {
            return false;
        }

        h5file = new H5::H5File(filename, H5F_ACC_TRUNC);

        if(h5file == nullptr) {
            return false;
        }

        elements_group = h5file->createGroup("/elements");
        return true;
    }

    bool H5Manager::initialize(const std::vector<std::string>& keys) {
        if(!isOpen()) {
            return false;
        }

        std::copy(keys.begin(), keys.end(), std::back_inserter(this->keys));

        return true;
    }

    void H5Manager::makeDatasets(hsize_t chunkSize) {
        hsize_t dim[] = { chunkSize };
        hsize_t maxdim[] = { H5S_UNLIMITED };

        H5::DataSpace dataspace(1, dim, maxdim);

        H5::DSetCreatPropList prop;
        prop.setChunk(1, dim);

        for(auto key_it = keys.begin(); key_it != keys.end(); key_it++) {
            datasets.push_back(
                elements_group.createDataSet(
                    key_it->c_str(), 
                    H5::PredType::IEEE_F64BE,
                    dataspace,
                    prop));
        }
    }

    bool H5Manager::close() {
        if(h5file == nullptr) {
            return true;
        }

        for(auto ds_it = datasets.begin(); ds_it != datasets.end(); ds_it++) {
            ds_it->close();
        }

        datasets.clear();
        keys.clear();
        elements_group.close();
        h5file->close();

        return true;
    }

    bool H5Manager::isOpen() {
        return h5file != nullptr;
    }

    bool H5Manager::writeData(double * data, hsize_t chunkSize, hsize_t currentChunk) {
        if(datasets.empty()) {
            // We haven't seen the datasets yet
            makeDatasets(chunkSize);
        }
        else {
            // Need to resize to current usage
            hsize_t start, end;
            hsize_t length;
            for(auto ds_it = datasets.begin(); ds_it != datasets.end(); ds_it++) {
                // Extend the dataset
                ds_it->getSpace().getSelectBounds(&start, &end);
                length = (end - start) + chunkSize + 1;
                ds_it->extend(&length);
            }
        }

        // Setup the buffer space
        hsize_t start = 0;
        hsize_t bufferSize = chunkSize*datasets.size();
        hsize_t stride = datasets.size();

        H5::DataSpace bufferspace(1, &bufferSize, &bufferSize);

        for(auto dataset_it = datasets.begin(); dataset_it != datasets.end(); dataset_it++) {
            bufferspace.selectHyperslab(H5S_SELECT_SET, &chunkSize, &start, &stride);
            start += 1;

            H5::DataSpace filespace = dataset_it->getSpace();
            filespace.selectHyperslab(H5S_SELECT_SET, &chunkSize, &currentChunk);
            dataset_it->write(data, H5::PredType::NATIVE_DOUBLE, bufferspace, filespace);
        }

        return true;
    }
}
