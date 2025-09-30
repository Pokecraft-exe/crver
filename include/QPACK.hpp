namespace QPACK {
	namespace ErrorCode {
		enum class ErrorCode {
		};
	}
	namespace exception {
		class InvalidData : public std::exception {
		public:
			InvalidData(const std::string& message) : msg(message) {}
			const char* what() const noexcept override { return msg.c_str(); }
		private:
			std::string msg;
		};
	}
	typedef std::vector<uint8_t> EncodedBuffer;
	typedef std::vector<uint8_t> DecodedBuffer;

	class QPACK {
	private:
		EncodedBuffer EBuf;
		DecodedBuffer DBuf;
	public:
		QPACK() = default;
		QPACK(const std::vector<uint8_t>& data) {
			if (decode::is_encoded(data)) {
				EBuf = data;
			} else if (encode::in_encoded(data)) {
				DBuf = data;
			} else {
				throw exception::InvalidData("The entered data is invalid"); // Assuming InvalidData is defined in ErrorCode
			}
		}
		QPACK& operator=(const QPACK& other) {
			if (this != &other) {
				DBuf = other.DBuf;
				EBuf = other.EBuf;
			}
			return *this;
		}

	};

	namespace encode {
		ErrorCode::ErrorCode is_encoded(const std::vector<uint8_t>& data) {

			return !data.empty(); // Example condition, replace with actual logic
		}
		std::vector<uint8_t> encode(const std::vector<uint8_t>& data) {
			
			return data;
		}
	}

	namespace decode{

	}
}

QPACK::QPACK operator ""_QPACK(const char* str) {
	size_t len = strlen(str);
	std::vector<uint8_t> data;
	data.reserve(len);
	memcpy(data.data(), str, len);
	return QPACK::QPACK(data);
}