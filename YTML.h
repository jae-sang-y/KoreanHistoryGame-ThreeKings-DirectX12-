#pragma once

#define Float(x) std::stof(x)
#define Int(x) std::stoi(x)
#define Long(x) std::stoll(x)
#define Str(x) std::to_wstring(x)

namespace YTML
{
	class DrawItem
	{
	public:
		DrawItem() = default;
		DrawItem(std::wstring com, std::uint64_t _uuid) : uuid(_uuid)
		{
			Attribute[L"z-index"] = L"0";

			Attribute[L"color-r"] = L"1";
			Attribute[L"color-g"] = L"1";
			Attribute[L"color-b"] = L"1";

			Attribute[L"opacity"] = L"1";

			Attribute[L"left"] = L"0";
			Attribute[L"top"] = L"0";
			Attribute[L"width"] = L"64";
			Attribute[L"height"] = L"64";

			std::wstring buf = com;

			if (buf.at(0) == '<' && buf.at(buf.length() - 1) == '>')
			{
				buf = buf.substr(1, buf.length() - 2);

				Attribute[L"tag"] = buf.substr(0, buf.find(' '));

				buf = buf.substr(buf.find(' '));

				//Divided to Construct
				while(buf.size())
				{
					while (*buf.cbegin() == ' ')
						buf = buf.substr(1);

					size_t end = buf.find('"', buf.find('"') + 1);

					if (end == std::wstring::npos)
						break;

					
					std::wstring construct = buf.substr(0, end + 1);
					{
						size_t sign = construct.find('=');
						int a = 0;
						//Set RValue
						if (sign == std::wstring::npos)
						{
							Attribute[construct] = L"";
						}
						else
						{

							std::wstring rvalue = construct.substr(sign + 1);
							if (!(rvalue.at(0) == '"' && rvalue.at(rvalue.length() - 1) == '"'))
								rvalue = L"";
							else
								rvalue = rvalue.substr(1, rvalue.length() - 2);
							
							Attribute[construct.substr(0, sign)] = rvalue;
						}


					}

					buf = buf.substr(end + 1);
				}

			}
			else
			{
				//Corrupt
			}
		}
		std::unordered_map<std::wstring, std::wstring> Attribute;
		float z_index = 0.f;
		const std::uint64_t uuid;
	};
	
	class DrawItemList 
	{
	private:
		std::uint64_t uuid_progress = 0;
	public:
		std::list<YTML::DrawItem> data;
		void Sort()
		{
			for (auto& O : data)
			{
				try
				{
					O.z_index = Float(O.Attribute[L"z-index"]);
				}
				catch (const std::exception&)
				{
					O.z_index = 0.f;
				}
			}
			
			data.sort([](YTML::DrawItem left, YTML::DrawItem right)
			{
				return left.z_index < right.z_index;
			});
		}

		void Insert(wchar_t* tag)
		{
			data.push_back(YTML::DrawItem(tag, ++uuid_progress));
			
			Sort();
		}

		/*void withClass(const std::wstring& className, std::initializer_list<std::wstring> args)
		{
			for (auto& O : data)
				if (O->Attribute[L"class"] == className)
					for (size_t i = 0; i < N / 2; ++i)
						O->Attribute[args[i * 2]] = args[i * 2 + 1];
		}*/

		std::list<YTML::DrawItem>::iterator withID(const std::wstring& id)
		{
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->Attribute[L"id"] == id)
				{
					return O;
				}
			return data.end();
		}
		std::list<YTML::DrawItem>::iterator withID(const std::wstring& id, std::initializer_list<std::wstring> args)
		{
			std::wstring head;
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->Attribute[L"id"] == id)
				{
					for (auto P = args.begin();;)
					{
						if (P == args.end()) break;
						head = *(P++);
						if (P == args.end()) break;
						O->Attribute[head] = *(P++);
					}
					return O;
				}
			return data.end();
		}

		std::list<YTML::DrawItem>::iterator withUUID(const std::uint64_t& uuid)
		{
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->uuid == uuid)
				{
					return O;
				}
			return data.end();
		}
		std::list<YTML::DrawItem>::iterator withUUID(const std::uint64_t& uuid, std::initializer_list<std::wstring> args)
		{
			std::wstring head;
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->uuid == uuid)
				{
					for (auto P = args.begin();;)
					{
						if (P == args.end()) break;
						head = *(P++);
						if (P == args.end()) break;
						O->Attribute[head] = *(P++);
					}
					return O;
				}
			return data.end();
		}

		std::list<std::list<YTML::DrawItem>::iterator> withClass(const std::wstring& classname)
		{
			std::list<std::list<YTML::DrawItem>::iterator> _return;
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->Attribute[L"class"] == classname)
				{
					_return.push_back(O);
				}
			return _return;
		}
		std::list<std::list<YTML::DrawItem>::iterator> withClass(const std::wstring& classname, std::initializer_list<std::wstring> args)
		{
			std::wstring head;
			std::list<std::list<YTML::DrawItem>::iterator> _return;
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->Attribute[L"class"] == classname)
				{
					for (auto P = args.begin();;)
					{
						if (P == args.end()) break;
						head = *(P++);
						if (P == args.end()) break;
						O->Attribute[head] = *(P++);
					}
					_return.push_back(O);
				}
			return _return;
		}
	};

}