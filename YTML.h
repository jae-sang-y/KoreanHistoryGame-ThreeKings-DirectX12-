#pragma once

#define Float(x) std::stof(x)
#define Int(x) std::stoi(x)
#define Long(x) std::stoll(x)
#define Str(x) std::to_wstring(x)

namespace YTML
{
	std::vector<std::wstring> Split(const std::wstring& wstr)
	{
		std::vector<std::wstring> _Return;
		size_t offset = 0;
		size_t baricade = 0;

		while (offset != std::wstring::npos)
		{
			offset = wstr.find(L' ', offset + 1);
			_Return.push_back(wstr.substr(baricade, offset - baricade));
			baricade = offset + 1;
		}

		return _Return;
	}

	/*template<typename T>
	std::wstring operator+(std::wstring Tx, T Ty)
	{
		return Tx + std::to_wstring(Ty);
	}
	template<typename T>
	std::wstring operator+(T Tx, std::wstring Ty)
	{
		return std::to_wstring(Ty) + Tx;
	}*/

	class DrawItem
	{
	private:
		std::unordered_map<std::wstring, std::wstring> Attribute;
	public:
		DrawItem() = default;
		DrawItem(std::wstring com, const std::uint64_t& _uuid, const std::uint64_t& _parent = 0) : uuid(_uuid)
		{
			parent = _parent;
			Attribute[L"inherit-z-index"] = L"0";
			Attribute[L"z-index"] = L"0";

			Attribute[L"background"] = L"disable";
			Attribute[L"background-color-r"] = L"1";
			Attribute[L"background-color-g"] = L"1";
			Attribute[L"background-color-b"] = L"1";
			Attribute[L"border"] = L"disable";
			Attribute[L"color-r"] = L"1";
			Attribute[L"color-g"] = L"1";
			Attribute[L"color-b"] = L"1";
			Attribute[L"opacity"] = L"1";

			Attribute[L"left"] = L"0";
			Attribute[L"top"] = L"0";
			Attribute[L"position-left"] = L"0";
			Attribute[L"position-top"] = L"0";

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

		std::wstring& operator[] (const std::wstring& index) { return Attribute[index]; }


		float z_index = 0.f;
		const std::uint64_t uuid;
		std::uint64_t parent;
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
					O.z_index = Float(O[L"z-index"]) + Float(O[L"inherit-z-index"]);
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


		class Query
		{
		public:
			std::list<std::list<YTML::DrawItem>::iterator> content;
			Query css(std::initializer_list<std::wstring> args)
			{
				std::wstring head;
				for (auto P = args.begin();;)
				{
					if (P == args.end()) break;
					head = *(P++);
					if (P == args.end()) break;
					for (auto& O : content)
					{
						(*O)[head] = *(P++);
					}
				}

				return *this;
			}
			std::list<std::list<YTML::DrawItem>::iterator>::iterator begin() { return content.begin(); }
			std::list<std::list<YTML::DrawItem>::iterator>::iterator end() { return content.end(); }
			operator std::uint64_t() {
				
				return content.size() > 0 ? (*content.begin())->uuid : 0;
			}
		};
		enum class SelectorType
		{
			UUID,
			ID,
			CLASS
		};
		Query $(std::wstring wstr)
		{
			Query _Return;
			auto S = Split(wstr);

			std::list<std::list<YTML::DrawItem>::iterator> buffer;
			YTML::DrawItemList::SelectorType lastsec = YTML::DrawItemList::SelectorType::UUID;

			if (S.size() > 0)
			{
				if (S.at(0).at(0) == L'#')
				{
					lastsec = YTML::DrawItemList::SelectorType::ID;
					S.at(0) = S.at(0).substr(1);
				}
				else if (S.at(0).at(0) == L'.')
				{
					lastsec = YTML::DrawItemList::SelectorType::CLASS;
					S.at(0) = S.at(0).substr(1);
				}
				else if (S.at(0).at(0) == L'@')
				{
					lastsec = YTML::DrawItemList::SelectorType::UUID;
					S.at(0) = S.at(0).substr(1);
				}
				switch (lastsec)
				{
				case YTML::DrawItemList::SelectorType::UUID:
				{
					std::uint64_t uuid = std::stoull(S.at(0));
					for (auto O = data.begin(); O != data.end(); ++O)
						if (O->uuid == uuid)
						{
							_Return.content.push_back(O);
							break;
						}
				}
					break;
				case YTML::DrawItemList::SelectorType::ID:
					for (auto O = data.begin(); O != data.end(); ++O)
						if ((*O)[L"id"] == S.at(0))
						{
							_Return.content.push_back(O);
							break;
						}
					break;
				case YTML::DrawItemList::SelectorType::CLASS:
					for (auto O = data.begin(); O != data.end(); ++O)
						if ((*O)[L"class"] == S.at(0))
						{
							_Return.content.push_back(O);
						}
					break;
				}
				for (size_t i = 1; i < S.size(); ++i)
				{
					std::swap(buffer, _Return.content);
					_Return.content.clear();

					if (S.at(i).at(0) == L'#')
					{
						lastsec = YTML::DrawItemList::SelectorType::ID;
						S.at(i) = S.at(i).substr(1);
					}
					else if (S.at(i).at(0) == L'.')
					{
						lastsec = YTML::DrawItemList::SelectorType::CLASS;
						S.at(i) = S.at(i).substr(1);
					}
					else if (S.at(i).at(0) == L'@')
					{
						lastsec = YTML::DrawItemList::SelectorType::UUID;
						S.at(i) = S.at(i).substr(1);
					}

					bool hard_break = true;
					switch (lastsec)
					{
					case YTML::DrawItemList::SelectorType::UUID:
					{
						std::uint64_t uuid = std::stoull(S.at(i));
						for (auto O = data.begin(); O != data.end() && hard_break; ++O)
							if (O->uuid == uuid && O->parent != 0)
							{
								for (auto P : buffer)
								{
									if (P->uuid == O->parent)
									{
										_Return.content.push_back(O);
										break;
									}
								}
								break;
							}
					}
						break;
					case YTML::DrawItemList::SelectorType::ID:
						for (auto O = data.begin(); O != data.end() && hard_break; ++O)
							if ((*O)[L"id"] == S.at(i) && O->parent != 0)
							{
								for (auto P : buffer)
								{
									if (P->uuid == O->parent)
									{
										_Return.content.push_back(O);
										hard_break = false;
										break;
									}
								}
							}
						break;
					case YTML::DrawItemList::SelectorType::CLASS:
						if (S.at(i) == L".")
						{
							for (auto P : buffer)
							{
								hard_break = true;
								if (P->parent > 0)
									for (auto O : withUUID(P->parent))
									{
										_Return.content.push_back(O);
										hard_break = false;
									}
								if (hard_break)
									_Return.content.push_back(P);
							}
						}
						else
						{
							for (auto O = data.begin(); O != data.end() && hard_break; ++O)
								if ((*O)[L"class"] == S.at(i) && O->parent != 0)
								{
									for (auto P : buffer)
									{
										if (P->uuid == O->parent)
										{
											_Return.content.push_back(O);
											hard_break = false;
											break;
										}
									}
								}
						}
						break;
					}
				}
			}



			return _Return;
		}

		Query Insert(wchar_t* tag, const std::uint64_t& parent = 0)
		{
			Query _Return;
			data.push_back(YTML::DrawItem(tag, ++uuid_progress, parent));
			_Return.content.push_back((++data.rbegin()).base());
			Sort();
			return _Return;
		}

		Query withUUID(const std::uint64_t& uuid)
		{
			Query _Return;
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->uuid == uuid)
				{
					_Return.content.push_back(O);
					break;
				}
			return _Return;
		}
		Query withUUID(const std::uint64_t& uuid, std::initializer_list<std::wstring> args)
		{
			Query _Return;
			std::wstring head;
			for (auto O = data.begin(); O != data.end(); ++O)
				if (O->uuid == uuid)
				{
					for (auto P = args.begin();;)
					{
						if (P == args.end()) break;
						head = *(P++);
						if (P == args.end()) break;
						(*O)[head] = *(P++);
					}
					_Return.content.push_back(O);
					break;
				}
			return _Return;
		}
		
	};

}