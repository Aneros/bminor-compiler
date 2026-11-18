# B-Minor — فاز اول کامپایلر

پیاده‌سازی با C++17 و کتابخانهٔ استاندارد: تحلیل لغوی، تحلیل نحوی، AST، بازیابی خطا و رنگ‌آمیزی وابسته به AST در ANSI و HTML.

## کنترل نسخه

مخزن Git محلی ایجاد شده است. اتصال به مخزن خصوصی GitHub/GitLab و اعطای دسترسی به ارائه‌گیرنده نیازمند تنظیمات صاحب حساب است. راهنمای ادامهٔ توسعه و اتصال مخزن در [مستندات کنترل نسخه](docs/development.md) آمده است.

## شروع سریع

نیازمندی اجرا: کامپایلر سازگار با C++17 مانند Clang یا GCC و ابزار Make. برای خود برنامه هیچ کتابخانهٔ خارجی، Python، Flex یا Bison لازم نیست. Python 3.10 به بالا فقط برای آزمون‌های یکپارچه استفاده می‌شود.

از ریشهٔ همین پوشه اجرا کنید:

```sh
make -j4
./bminor --check examples/valid/showcase.bminor
./bminor --highlight=html examples/valid/showcase.bminor -o out.html
```

فایل `out.html` را در مرورگر باز کنید؛ مستقل است و به سرور یا JavaScript نیاز ندارد. دو خروجی آماده نیز موجود است: [نمونهٔ معتبر](examples/showcase.html) و [نمایش خطاها و بازیابی](examples/errors.html).

## قابلیت‌ها

- Lexer دستی با Maximal Munch، تقدم کلمات کلیدی، حفظ لکسم، توضیحات، فاصله‌ها و موقعیت دقیق فایل/خط/ستون.
- Parser بازگشتی نزولی با پوشش اعلان متغیر، آرایه، نوع تابع، prototype، تعریف تابع، شرط، حلقهٔ `for`، `print`، `return`، بلوک، فراخوانی و عبارت‌ها.
- گرامر کامل [EBNF](grammar/bminor.ebnf)، توضیح تقدم و شرکت‌پذیری عملگرها و رفع ابهام `else`.
- AST مالک گره‌ها با `std::unique_ptr`، محدودهٔ منبع، اتصال به توکن و فیلد تهی `type_annotation` برای فاز بعد.
- تشخیص چند خطای لغوی و نحوی در یک اجرا؛ بازیابی Panic Mode و نگهداری AST جزئی.
- رنگ‌آمیزی نام تابع در اعلان و فراخوانی با استفاده از AST؛ قالب‌های ANSI و HTML/CSS با حفظ متن ورودی.
- خروجی JSON برای توکن‌ها، AST و خطاها، همراه با آزمون‌های C++، آزمون رابط خط فرمان و آزمون ورودی‌های تصادفی.

## اجرای بخش‌های مختلف

```sh
# توکن‌ها با نوع، لکسم و موقعیت؛ توضیحات هم نمایش داده می‌شوند
./bminor --tokens examples/valid/showcase.bminor

# تمام توکن‌ها، شامل فاصله‌ها، در JSON
./bminor --tokens --json --trivia examples/valid/showcase.bminor

# AST در JSON یا نمایش درختی متنی
./bminor --ast examples/valid/showcase.bminor
./bminor --ast=text examples/valid/showcase.bminor

# رنگ‌آمیزی ترمینال و مرورگر
./bminor --ansi examples/valid/showcase.bminor
./bminor --html examples/valid/showcase.bminor -o out.html

# گزارش خطاهای ساختاریافته روی stderr؛ خروجی HTML حتی با وجود خطا ساخته می‌شود
./bminor --html --diagnostics=json examples/invalid/syntax_errors.bminor -o errors.html

# خواندن از ورودی استاندارد
printf 'answer: integer = 42;\n' | ./bminor --ast --stdin-name input.bminor -

./bminor --help
```

بدون انتخاب حالت، `--check` اجرا می‌شود. `--tokens` فقط تحلیل لغوی انجام می‌دهد؛ سایر حالت‌ها Parser را نیز اجرا می‌کنند. خطاها روی `stderr` و نتیجهٔ عملیات روی `stdout` نوشته می‌شوند؛ `-o` نتیجه را به فایل می‌فرستد. مسیر خروجی نباید خودِ فایل ورودی یا پیوندی به آن باشد.

| کد خروج | معنی |
| --- | --- |
| 0 | بدون خطای تشخیص‌داده‌شده |
| 1 | خطای لغوی یا نحوی؛ تحلیل ادامه یافته و خروجی جزئی/رنگی تولید شده است |
| 2 | خطای آرگومان یا خواندن/نوشتن فایل |

## ساخت و آزمایش

```sh
make test         # آزمون‌های C++؛ بدون نیاز به Python
make check        # آزمون‌های C++ و آزمون‌های یکپارچهٔ CLI
make sanitize     # همان آزمون‌ها با AddressSanitizer و UndefinedBehaviorSanitizer
```

ساخت جایگزین با CMake نیز تعریف شده است:

```sh
cmake -S . -B build/cmake
cmake --build build/cmake --parallel
ctest --test-dir build/cmake --output-on-failure
```

روش Make و sanitizer روی Apple Clang 21 آزمایش شده‌اند. CMake روی محیط فعلی نصب نیست؛ مسیر آن برای اجرا در CI تعریف شده است. نتیجهٔ دقیق بررسی‌ها در [گزارش آزمون](docs/testing.md) آمده است.

## ساختار و مستندات

```text
include/bminor/     API و ساختارهای داده
src/               تمام کدهای اصلی، به زبان C++17
grammar/           گرامر کامل EBNF
tests/             آزمون‌های C++ و آزمون CLI با Python استاندارد
examples/valid/    برنامه‌های معتبر
examples/invalid/  ورودی‌های عمداً خطادار
docs/              قواعد، طراحی، پوشش الزامات و گزارش آزمون
.github/workflows/ تنظیمات CI برای مخزن آینده
```

- [گزارش فنی فارسی و معماری](docs/report-fa.md)
- [قواعد رسمی لغوی و روش DFA](docs/lexer.md)
- [گرامر، FIRST/FOLLOW و تصمیم‌های Parser](docs/grammar.md)
- [نگاشت الزامات به پیاده‌سازی و آزمون](docs/requirements.md)
- [آزمون‌ها و نتیجهٔ بررسی](docs/testing.md)
- [کنترل نسخه و اتصال مخزن خصوصی](docs/development.md)

## دامنهٔ زبان

صورت‌پروژهٔ محلی الزامات اصلی است. چون ضمیمه فقط نمادگذاری EBNF را شرح می‌دهد، جزئیات زبان از [ضمیمهٔ B کتاب Douglas Thain](https://dthain.github.io/books/compiler/chapterB.pdf) استخراج می‌شود. تفاوت نسخه‌های سالانهٔ زبان به‌صورت خودکار وارد این پیاده‌سازی نمی‌شود.

این فاز تحلیل لغوی و نحوی است؛ نام‌یابی، بررسی نوع، الزام وجود `main`، گراف جریان کنترل و تولید کد ماشین به فازهای بعد تعلق دارند. `--check` صحت نوع‌ها یا اجرای برنامه را تأیید نمی‌کند. در گویش کتاب، `while` رزرو شده ولی دستور حلقه نیست، عدد اعشاری و `auto` وجود ندارد و escapeهای رشته به صورت `\n`، `\0` و `\X → X` تعریف می‌شوند. این انتخاب‌ها همراه با موارد مبهم مرجع در مستندات ثبت شده‌اند.

