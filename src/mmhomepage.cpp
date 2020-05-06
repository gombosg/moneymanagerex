/*******************************************************
Copyright (C) 2014 - 2020 Nikolay Akimov

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ********************************************************/

#include "mmhomepage.h"
#include "html_template.h"
#include "billsdepositspanel.h"
#include "constants.h"
#include <algorithm>
#include <cmath>

#include "model/Model_Stock.h"
#include "model/Model_StockHistory.h"
#include "model/Model_Category.h"
#include "model/Model_Currency.h"
#include "model/Model_CurrencyHistory.h"
#include "model/Model_Payee.h"
#include "model/Model_Asset.h"
#include "model/Model_Setting.h"

static const wxString TOP_CATEGS = R"(
<table class = 'table'>
  <tr class='active'>
    <th>%s</th>
    <th nowrap class='text-right sorttable_nosort'>
      <a id='%s_label' onclick='toggleTable("%s"); ' href='#%s' oncontextmenu='return false;'>[-]</a>
    </th>
  </tr>
  <tr>
    <td style='padding: 0px; padding-left: 0px; padding-right: 0px; width: 100%%;' colspan='2'>
    <table class = 'sortable table' id='%s'>
    <thead>
      <tr><th>%s</th><th class='text-right'>%s</th></tr>
    </thead>
   <tbody>
%s
   </tbody>
</table>
</td></tr>
</table>
)";


htmlWidgetStocks::htmlWidgetStocks()
    : title_(_("Stocks"))
{
    grand_gain_lost_ = 0.0;
    grand_total_ = 0.0;
}

htmlWidgetStocks::~htmlWidgetStocks()
{
}

const wxString htmlWidgetStocks::getHTMLText()
{
    wxString output = "";
    std::map<int, std::pair<double, double> > stockStats;
    calculate_stats(stockStats);
    if (!stockStats.empty())
    {
        output = "<table class ='sortable table'><col style='width: 50%'><col style='width: 25%'><col style='width: 25%'><thead><tr class='active'><th>\n";
        output += _("Stocks") + "</th><th class = 'text-right'>" + _("Gain/Loss");
        output += "</th>\n<th class='text-right'>" + _("Total") + "</th>\n";
        output += wxString::Format("<th nowrap class='text-right sorttable_nosort'><a id='%s_label' onclick='toggleTable(\"%s\");' href='#%s' oncontextmenu='return false;'>[-]</a></th>\n"
            , "INVEST", "INVEST", "INVEST");
        output += "</tr></thead><tbody id='INVEST'>\n";
        const auto &accounts = Model_Account::instance().all(Model_Account::COL_ACCOUNTNAME);
        wxString body = "";
        for (const auto& account : accounts)
        {
            if (Model_Account::type(account) != Model_Account::INVESTMENT) continue;
            if (Model_Account::status(account) != Model_Account::OPEN) continue;
            body += "<tr>";
            body += wxString::Format("<td sorttable_customkey='*%s*'><a href='stock:%i' oncontextmenu='return false;'>%s</a></td>\n"
                , account.ACCOUNTNAME, account.ACCOUNTID, account.ACCOUNTNAME);
            body += wxString::Format("<td class='money' sorttable_customkey='%f'>%s</td>\n"
                , stockStats[account.ACCOUNTID].first
                , Model_Account::toCurrency(stockStats[account.ACCOUNTID].first, &account));
            body += wxString::Format("<td colspan='2' class='money' sorttable_customkey='%f'>%s</td>"
                , stockStats[account.ACCOUNTID].second
                , Model_Account::toCurrency(stockStats[account.ACCOUNTID].second, &account));
            body += "</tr>";
        }

        output += body;
        output += "</tbody><tfoot><tr class = 'total'><td>" + _("Total:") + "</td>";
        output += wxString::Format("<td class='money'>%s</td>"
            , Model_Currency::toCurrency(grand_gain_lost_));
        output += wxString::Format("<td colspan='2' class='money'>%s</td></tr></tfoot></table>"
            , Model_Currency::toCurrency(grand_total_));
        if (body.empty()) output.clear();
    }
    return output;
}

void htmlWidgetStocks::calculate_stats(std::map<int, std::pair<double, double> > &stockStats)
{
    this->grand_total_ = 0;
    this->grand_gain_lost_ = 0;
    const auto &stocks = Model_Stock::instance().all();
    const wxDate today = wxDate::Today();
    for (const auto& stock : stocks)
    {
        double conv_rate = 1;
        Model_Account::Data *account = Model_Account::instance().get(stock.HELDAT);
        if (account)
        {
            conv_rate = Model_CurrencyHistory::getDayRate(account->CURRENCYID, today);
        }
        std::pair<double, double>& values = stockStats[stock.HELDAT];
        double current_value = Model_Stock::CurrentValue(stock);
        double gain_lost = (current_value - stock.VALUE - stock.COMMISSION);
        values.first += gain_lost;
        values.second += current_value;
        if (account && account->STATUS == VIEW_ACCOUNTS_OPEN_STR)
        {
            grand_total_ += current_value * conv_rate;
            grand_gain_lost_ += gain_lost * conv_rate;
        }
    }
}

double htmlWidgetStocks::get_total()
{
    return grand_total_;
}

double htmlWidgetStocks::get_total_gein_lost()
{
    return grand_gain_lost_;
}

////////////////////////////////////////////////////////


htmlWidgetTop7Categories::htmlWidgetTop7Categories()
{
    date_range_ = new mmLast30Days();
    title_ = wxString::Format(_("Top Withdrawals: %s"), date_range_->local_title());
}

htmlWidgetTop7Categories::~htmlWidgetTop7Categories()
{
    if (date_range_) delete date_range_;
}

const wxString htmlWidgetTop7Categories::getHTMLText()
{

    std::vector<std::pair<wxString, double> > topCategoryStats;
    getTopCategoryStats(topCategoryStats, date_range_);
    wxString output = "", data;

    if (!topCategoryStats.empty())
    {
        for (const auto& i : topCategoryStats)
        {
            data += "<tr>";
            data += wxString::Format("<td>%s</td>", (i.first.IsEmpty() ? "..." : i.first));
            data += wxString::Format("<td class='money' sorttable_customkey='%f'>%s</td>\n"
                , i.second
                , Model_Currency::toCurrency(i.second));
            data += "</tr>\n";
        }
        const wxString idStr = "TOP_CATEGORIES";
        output += wxString::Format(TOP_CATEGS, title_, idStr, idStr, idStr, idStr, _("Category"), _("Summary"), data);
    }

    return output;
}

void htmlWidgetTop7Categories::getTopCategoryStats(
    std::vector<std::pair<wxString, double> > &categoryStats
    , const mmDateRange* date_range) const
{
    //Get base currency rates for all accounts
    std::map<int, double> acc_conv_rates;
    const wxDate today = wxDate::Today();
    for (const auto& account : Model_Account::instance().all())
    {
        acc_conv_rates[account.ACCOUNTID] = Model_CurrencyHistory::getDayRate(account.CURRENCYID, today);
    }
    //Temporary map
    std::map<std::pair<int /*category*/, int /*sub category*/>, double> stat;

    const auto splits = Model_Splittransaction::instance().get_all();
    const auto &transactions = Model_Checking::instance().find(
        Model_Checking::TRANSDATE(date_range->start_date(), GREATER_OR_EQUAL)
        , Model_Checking::TRANSDATE(date_range->end_date(), LESS_OR_EQUAL)
        , Model_Checking::STATUS(Model_Checking::VOID_, NOT_EQUAL)
        , Model_Checking::TRANSCODE(Model_Checking::TRANSFER, NOT_EQUAL));

    for (const auto &trx : transactions)
    {
        bool withdrawal = Model_Checking::type(trx) == Model_Checking::WITHDRAWAL;
        const auto it = splits.find(trx.TRANSID);

        if (it == splits.end())
        {
            std::pair<int, int> category = std::make_pair(trx.CATEGID, trx.SUBCATEGID);
            if (withdrawal)
                stat[category] -= trx.TRANSAMOUNT * (acc_conv_rates[trx.ACCOUNTID]);
            else
                stat[category] += trx.TRANSAMOUNT * (acc_conv_rates[trx.ACCOUNTID]);
        }
        else
        {
            for (const auto& entry : it->second)
            {
                std::pair<int, int> category = std::make_pair(entry.CATEGID, entry.SUBCATEGID);
                double val = entry.SPLITTRANSAMOUNT
                    * (acc_conv_rates[trx.ACCOUNTID])
                    * (withdrawal ? -1 : 1);
                stat[category] += val;
            }
        }
    }

    categoryStats.clear();
    for (const auto& i : stat)
    {
        if (i.second < 0)
        {
            std::pair <wxString, double> stat_pair;
            stat_pair.first = Model_Category::full_name(i.first.first, i.first.second);
            stat_pair.second = i.second;
            categoryStats.push_back(stat_pair);
        }
    }

    std::stable_sort(categoryStats.begin(), categoryStats.end()
        , [](const std::pair<wxString, double> x, const std::pair<wxString, double> y)
    { return x.second < y.second; }
    );

    int counter = 0;
    std::vector<std::pair<wxString, double> >::iterator iter;
    for (iter = categoryStats.begin(); iter != categoryStats.end(); )
    {
        counter++;
        if (counter > 7)
            iter = categoryStats.erase(iter);
        else
            ++iter;
    }
}

////////////////////////////////////////////////////////


htmlWidgetBillsAndDeposits::htmlWidgetBillsAndDeposits(const wxString& title, mmDateRange* date_range)
    : title_(title)
    , date_range_(date_range)
{}

htmlWidgetBillsAndDeposits::~htmlWidgetBillsAndDeposits()
{
    if (date_range_) delete date_range_;
}

const wxString htmlWidgetBillsAndDeposits::getHTMLText()
{
    wxString output = "";
    wxDate today = wxDate::Today();

    //                    days, payee, description, amount, account
    std::vector< std::tuple<int, wxString, wxString, double, const Model_Account::Data*> > bd_days;
    for (const auto& entry : Model_Billsdeposits::instance().all(Model_Billsdeposits::COL_NEXTOCCURRENCEDATE))
    {
        int daysPayment = Model_Billsdeposits::NEXTOCCURRENCEDATE(&entry)
            .Subtract(today).GetDays();
        if (daysPayment > 14)
            break; // Done searching for all to include

        int repeats = entry.REPEATS;
        // DeMultiplex the Auto Executable fields.
        if (repeats >= BD_REPEATS_MULTIPLEX_BASE)    // Auto Execute User Acknowlegement required
            repeats -= BD_REPEATS_MULTIPLEX_BASE;
        if (repeats >= BD_REPEATS_MULTIPLEX_BASE)    // Auto Execute Silent mode
            repeats -= BD_REPEATS_MULTIPLEX_BASE;

        if (daysPayment == 0 && repeats > 10 && repeats < 15 && entry.NUMOCCURRENCES < 0) {
            continue; // Inactive
        }

        int daysOverdue = Model_Billsdeposits::TRANSDATE(&entry)
            .Subtract(today).GetDays();
        wxString daysRemainingStr = (daysPayment > 0
            ? wxString::Format(wxPLURAL("%d day remaining", "%d days remaining", daysPayment), daysPayment)
            : wxString::Format(wxPLURAL("%d day delay!", "%d days delay!", -daysPayment), -daysPayment));
        if (daysOverdue < 0)
            daysRemainingStr = wxString::Format(wxPLURAL("%d day overdue!", "%d days overdue!", std::abs(daysOverdue)), std::abs(daysOverdue));

        wxString payeeStr = "";
        if (Model_Billsdeposits::type(entry) == Model_Billsdeposits::TRANSFER)
        {
            const Model_Account::Data *account = Model_Account::instance().get(entry.TOACCOUNTID);
            if (account) payeeStr = account->ACCOUNTNAME;
        }
        else
        {
            const Model_Payee::Data* payee = Model_Payee::instance().get(entry.PAYEEID);
            if (payee) payeeStr = payee->PAYEENAME;
        }
        const auto *account = Model_Account::instance().get(entry.ACCOUNTID);
        double amount = (Model_Billsdeposits::type(entry) == Model_Billsdeposits::DEPOSIT ? entry.TRANSAMOUNT : -entry.TRANSAMOUNT);
        bd_days.push_back(std::make_tuple(daysPayment, payeeStr, daysRemainingStr, amount, account));
    }

    //std::sort(bd_days.begin(), bd_days.end());
    //std::reverse(bd_days.begin(), bd_days.end());
    ////////////////////////////////////

    if (!bd_days.empty())
    {
        static const wxString idStr = "BILLS_AND_DEPOSITS";

        output = "<table class='table'>\n<thead>\n<tr class='active'><th>";
        output += wxString::Format("<a href=\"billsdeposits:\" oncontextmenu='return false;'>%s</a></th>\n<th></th>\n", title_);
        output += wxString::Format("<th nowrap class='text-right sorttable_nosort'>%i <a id='%s_label' onclick=\"toggleTable('%s'); \" href='#%s' oncontextmenu='return false;'>[-]</a></th></tr>\n"
            , int(bd_days.size()), idStr, idStr, idStr);
        output += "</thead>\n";

        output += wxString::Format("<tbody id='%s'>\n", idStr);
        output += wxString::Format("<tr style='background-color: #d8ebf0'><th>%s</th>\n<th class='text-right'>%s</th>\n<th class='text-right'>%s</th></tr>\n"
            , _("Payee"), _("Amount"), _("Payment"));

        for (const auto& item : bd_days)
        {
            output += wxString::Format("<tr %s>\n", std::get<0>(item) < 0 ? "class='danger'" : "");
            output += "<td>" + std::get<1>(item) + "</td>"; //payee
            output += wxString::Format("<td class='money'>%s</td>\n"
                , Model_Account::toCurrency(std::get<3>(item), std::get<4>(item)));
            output += "<td  class='money'>" + std::get<2>(item) + "</td></tr>\n";
        }
        output += "</tbody></table>\n";
    }
    return output;
}

////////////////////////////////////////////////////////

//* Income vs Expenses *//
const wxString htmlWidgetIncomeVsExpenses::getHTMLText()
{
    wxSharedPtr<mmDateRange> date_range;
    if (Option::instance().getIgnoreFutureTransactions())
        date_range = new mmCurrentMonthToDate;
    else
        date_range = new mmCurrentMonth;

    double tIncome = 0.0, tExpenses = 0.0;
    std::map<int, std::pair<double, double> > incomeExpensesStats;

    //Initialization
    bool ignoreFuture = Option::instance().getIgnoreFutureTransactions();

    //Calculations
    const auto &transactions = Model_Checking::instance().find(
        Model_Checking::TRANSDATE(date_range->start_date(), GREATER_OR_EQUAL)
        , Model_Checking::TRANSDATE(date_range->end_date(), LESS_OR_EQUAL)
        , Model_Checking::STATUS(Model_Checking::VOID_, NOT_EQUAL)
        , Model_Checking::TRANSCODE(Model_Checking::TRANSFER, NOT_EQUAL)
    );

    // Account ID, currency rate for today
    std::map<int, double> curencyRates;
    wxDate todayDate = wxDate::Today();
    for (const auto& account : Model_Account::instance().all())
    {
        double convRate = Model_CurrencyHistory::getDayRate(Model_Account::currency(account)->CURRENCYID, todayDate);
        curencyRates[account.ACCOUNTID] = convRate;
    }

    for (const auto& pBankTransaction : transactions)
    {
        if (ignoreFuture)
        {
            if (Model_Checking::TRANSDATE(pBankTransaction).IsLaterThan(date_range->today()))
                continue; //skip future dated transactions
        }

        // Do not include asset or stock transfers in income expense calculations.
        if (Model_Checking::foreignTransactionAsTransfer(pBankTransaction))
            continue;

        double convRate = curencyRates[pBankTransaction.ACCOUNTID];

        int idx = pBankTransaction.ACCOUNTID;
        if (Model_Checking::type(pBankTransaction) == Model_Checking::DEPOSIT)
            incomeExpensesStats[idx].first += pBankTransaction.TRANSAMOUNT * convRate;
        else
            incomeExpensesStats[idx].second += pBankTransaction.TRANSAMOUNT * convRate;
    }

    for (const auto& account : Model_Account::instance().all())
    {
        int idx = account.ACCOUNTID;
        tIncome += incomeExpensesStats[idx].first;
        tExpenses += incomeExpensesStats[idx].second;
    }
    // Compute chart spacing and interval (chart forced to start at zero)
    double steps = 10.0;
    double scaleStepWidth = ceil(std::max(tIncome, tExpenses) / steps);
    if (scaleStepWidth <= 1.0)
        scaleStepWidth = 1.0;
    else {
        double s = (pow(10, ceil(log10(scaleStepWidth)) - 1.0));
        if (s > 0) scaleStepWidth = ceil(scaleStepWidth / s)*s;
    }

    StringBuffer json_buffer;
    PrettyWriter<StringBuffer> json_writer(json_buffer);
    json_writer.StartObject();
    json_writer.Key("0");
    json_writer.String(wxString::Format(_("Income vs Expenses: %s"), date_range.get()->local_title()).utf8_str());
    json_writer.Key("1");
    json_writer.String(_("Type").utf8_str());
    json_writer.Key("2");
    json_writer.String(_("Amount").utf8_str());
    json_writer.Key("3");
    json_writer.String(_("Income").utf8_str());
    json_writer.Key("4");
    json_writer.String(Model_Currency::toCurrency(tIncome).utf8_str());
    json_writer.Key("5");
    json_writer.String(_("Expenses").utf8_str());
    json_writer.Key("6");
    json_writer.String(Model_Currency::toCurrency(tExpenses).utf8_str());
    json_writer.Key("7");
    json_writer.String(_("Difference:").utf8_str());
    json_writer.Key("8");
    json_writer.String(Model_Currency::toCurrency(tIncome - tExpenses).utf8_str());
    json_writer.Key("9");
    json_writer.String(_("Income/Expenses").utf8_str());
    json_writer.Key("10");
    json_writer.String(wxString::FromCDouble(tIncome, 2).utf8_str());
    json_writer.Key("11");
    json_writer.String(wxString::FromCDouble(tExpenses, 2).utf8_str());
    json_writer.Key("12");
    json_writer.Int(steps);
    json_writer.Key("13");
    json_writer.Int(scaleStepWidth);
    json_writer.EndObject();

    wxLogDebug("======= mmHomePagePanel::getIncomeVsExpensesJSON =======");
    wxLogDebug("RapidJson\n%s", wxString::FromUTF8(json_buffer.GetString()));

    return wxString::FromUTF8(json_buffer.GetString());
}

htmlWidgetIncomeVsExpenses::~htmlWidgetIncomeVsExpenses()
{
}

const wxString htmlWidgetStatistics::getHTMLText()
{
    StringBuffer json_buffer;
    PrettyWriter<StringBuffer> json_writer(json_buffer);
    json_writer.StartObject();

    json_writer.Key("NAME");
    json_writer.String(_("Transaction Statistics").utf8_str());

    wxSharedPtr<mmDateRange> date_range;
    if (Option::instance().getIgnoreFutureTransactions())
        date_range = new mmCurrentMonthToDate;
    else
        date_range = new mmCurrentMonth;

    Model_Checking::Data_Set all_trans;
    if (Option::instance().getIgnoreFutureTransactions())
    {
        all_trans = Model_Checking::instance().find(
            DB_Table_CHECKINGACCOUNT_V1::TRANSDATE(date_range->today().FormatISODate(), LESS_OR_EQUAL));
    }
    else
    {
        all_trans = Model_Checking::instance().all();
    }
    int countFollowUp = 0;
    int total_transactions = all_trans.size();

    std::map<int, std::pair<double, double> > accountStats;
    for (const auto& trx : all_trans)
    {
        // Do not include asset or stock transfers in income expense calculations.
        if (Model_Checking::foreignTransactionAsTransfer(trx))
            continue;

        if (Model_Checking::status(trx) == Model_Checking::FOLLOWUP) countFollowUp++;

        accountStats[trx.ACCOUNTID].first += Model_Checking::reconciled(trx, trx.ACCOUNTID);
        accountStats[trx.ACCOUNTID].second += Model_Checking::balance(trx, trx.ACCOUNTID);

        if (Model_Checking::type(trx) == Model_Checking::TRANSFER)
        {
            accountStats[trx.TOACCOUNTID].first += Model_Checking::reconciled(trx, trx.TOACCOUNTID);
            accountStats[trx.TOACCOUNTID].second += Model_Checking::balance(trx, trx.TOACCOUNTID);
        }
    }


    if (countFollowUp > 0)
    {
        json_writer.Key(_("Follow Up On Transactions: ").utf8_str());
        json_writer.Double(countFollowUp);
    }

    json_writer.Key(_("Total Transactions: ").utf8_str());
    json_writer.Int(total_transactions);
    json_writer.EndObject();

    wxLogDebug("======= mmHomePagePanel::getStatWidget =======");
    wxLogDebug("RapidJson\n%s", wxString::FromUTF8(json_buffer.GetString()));

    return wxString::FromUTF8(json_buffer.GetString());
}

htmlWidgetStatistics::~htmlWidgetStatistics()
{
}

const wxString htmlWidgetGrandTotals::getHTMLText(double& tBalance)
{
    const wxString tBalanceStr = Model_Currency::toCurrency(tBalance);

    StringBuffer json_buffer;
    PrettyWriter<StringBuffer> json_writer(json_buffer);
    json_writer.StartObject();
    json_writer.Key("NAME");
    json_writer.String(_("Grand Total:").utf8_str());
    json_writer.Key("VALUE");
    json_writer.String(tBalanceStr.utf8_str());
    json_writer.EndObject();

    wxLogDebug("======= mmHomePagePanel::getGrandTotalsJSON =======");
    wxLogDebug("RapidJson\n%s", wxString::FromUTF8(json_buffer.GetString()));

    return wxString::FromUTF8(json_buffer.GetString());
}

htmlWidgetGrandTotals::~htmlWidgetGrandTotals()
{
}

const wxString htmlWidgetAssets::getHTMLText(double& tBalance)
{
    double asset_balance = Model_Asset::instance().balance();
    tBalance += asset_balance;

    StringBuffer json_buffer;
    PrettyWriter<StringBuffer> json_writer(json_buffer);
    json_writer.StartObject();
    json_writer.Key("NAME");
    json_writer.String(_("Assets").utf8_str());
    json_writer.Key("VALUE");
    json_writer.String(Model_Currency::toCurrency(asset_balance).utf8_str());
    json_writer.EndObject();

    wxLogDebug("======= mmHomePagePanel::getAssetsJSON =======");
    wxLogDebug("RapidJson\n%s", wxString::FromUTF8(json_buffer.GetString()));

    return wxString::FromUTF8(json_buffer.GetString());
}

htmlWidgetAssets::~htmlWidgetAssets()
{
}

//

htmlWidgetAccounts::htmlWidgetAccounts()
{
    get_account_stats();
}

void htmlWidgetAccounts::get_account_stats()
{

    wxSharedPtr<mmDateRange> date_range;
    if (Option::instance().getIgnoreFutureTransactions())
        date_range = new mmCurrentMonthToDate;
    else
        date_range = new mmCurrentMonth;

    Model_Checking::Data_Set all_trans;
    if (Option::instance().getIgnoreFutureTransactions())
    {
        all_trans = Model_Checking::instance().find(
            DB_Table_CHECKINGACCOUNT_V1::TRANSDATE(date_range->today().FormatISODate(), LESS_OR_EQUAL));
    }
    else
    {
        all_trans = Model_Checking::instance().all();
    }

    for (const auto& trx : all_trans)
    {
        // Do not include asset or stock transfers in income expense calculations.
        if (Model_Checking::foreignTransactionAsTransfer(trx))
            continue;

        accountStats_[trx.ACCOUNTID].first += Model_Checking::reconciled(trx, trx.ACCOUNTID);
        accountStats_[trx.ACCOUNTID].second += Model_Checking::balance(trx, trx.ACCOUNTID);

        if (Model_Checking::type(trx) == Model_Checking::TRANSFER)
        {
            accountStats_[trx.TOACCOUNTID].first += Model_Checking::reconciled(trx, trx.TOACCOUNTID);
            accountStats_[trx.TOACCOUNTID].second += Model_Checking::balance(trx, trx.TOACCOUNTID);
        }
    }

}

const wxString htmlWidgetAccounts::displayAccounts(double& tBalance, int type = Model_Account::CHECKING)
{
    static const std::vector < std::pair <wxString, wxString> > typeStr
    {
        { "CASH_ACCOUNTS_INFO", _("Cash Accounts") },
        { "ACCOUNTS_INFO", _("Bank Accounts") },
        { "CARD_ACCOUNTS_INFO", _("Credit Card Accounts") },
        { "LOAN_ACCOUNTS_INFO", _("Loan Accounts") },
        { "TERM_ACCOUNTS_INFO", _("Term Accounts") },
    };

    const wxString idStr = typeStr[type].first;
    wxString output = "<table class = 'sortable table'>\n";
    output += R"(<col style="width:50%"><col style="width:25%"><col style="width:25%">)";
    output += "<thead><tr><th nowrap>\n";
    output += typeStr[type].second;

    output += "</th><th class = 'text-right'>" + _("Reconciled") + "</th>\n";
    output += "<th class = 'text-right'>" + _("Balance") + "</th>\n";
    output += wxString::Format("<th nowrap class='text-right sorttable_nosort'><a id='%s_label' onclick=\"toggleTable('%s'); \" href='#%s' oncontextmenu='return false;'>[-]</a></th>\n"
        , idStr, idStr, idStr);
    output += "</tr></thead>\n";
    output += wxString::Format("<tbody id = '%s'>\n", idStr);

    double tReconciled = 0;
    wxString body = "";
    const wxDate today = wxDate::Today();
    wxString vAccts = Model_Setting::instance().ViewAccounts();
    for (const auto& account : Model_Account::instance().all(Model_Account::COL_ACCOUNTNAME))
    {
        if (Model_Account::type(account) != type || Model_Account::status(account) == Model_Account::CLOSED) continue;

        Model_Currency::Data* currency = Model_Account::currency(account);

        double currency_rate = Model_CurrencyHistory::getDayRate(account.CURRENCYID, today);
        double bal = account.INITIALBAL + accountStats_[account.ACCOUNTID].second; //Model_Account::balance(account);
        double reconciledBal = account.INITIALBAL + accountStats_[account.ACCOUNTID].first;
        tBalance += bal * currency_rate;
        tReconciled += reconciledBal * currency_rate;

        // show the actual amount in that account
        if (((vAccts == VIEW_ACCOUNTS_OPEN_STR && Model_Account::status(account) == Model_Account::OPEN) ||
            (vAccts == VIEW_ACCOUNTS_FAVORITES_STR && Model_Account::FAVORITEACCT(account)) ||
            (vAccts == VIEW_ACCOUNTS_ALL_STR)))
        {
            body += "<tr>";
            body += wxString::Format("<td sorttable_customkey='*%s*' nowrap><a href='acct:%i' oncontextmenu='return false;'>%s</a></td>\n"
                , account.ACCOUNTNAME, account.ACCOUNTID, account.ACCOUNTNAME);
            body += wxString::Format("<td class='money' sorttable_customkey='%f' nowrap>%s</td>\n", reconciledBal, Model_Currency::toCurrency(reconciledBal, currency));
            body += wxString::Format("<td class='money' sorttable_customkey='%f' colspan='2' nowrap>%s</td>\n", bal, Model_Currency::toCurrency(bal, currency));
            body += "</tr>\n";
        }
    }
    output += body;
    output += "</tbody><tfoot><tr class ='total'><td>" + _("Total:") + "</td>\n";
    output += "<td class='money'>" + Model_Currency::toCurrency(tReconciled) + "</td>\n";
    output += "<td class='money' colspan='2'>" + Model_Currency::toCurrency(tBalance) + "</td></tr></tfoot></table>\n";
    if (body.empty()) output.clear();

    return output;
}

htmlWidgetAccounts::~htmlWidgetAccounts()
{
}

// Currency exchange rates
const wxString htmlWidgetCurrency::getHtmlText()
{

    const char* currencyRatesTemplate = R"(
<div class = "container">
<b><TMPL_VAR FRAME_NAME></b>
<a id='CURRENCY_RATES_label' onclick='toggleTable("CURRENCY_RATES");' href='#CURRENCY_RATES' oncontextmenu='return false;'>[-]</a>
<table class="table" id='CURRENCY_RATES'>
<thead>
<tr><th></th> <TMPL_VAR HEADER></tr>
</thead>
<tbody>
<TMPL_LOOP NAME=CONTENTS>
<tr><td class ='success'><TMPL_VAR CURRENCY_SYMBOL></td><TMPL_VAR CONVERSION_RATE></tr>
</TMPL_LOOP>
</tbody>
</table>
</div>
)";


    const wxString today = wxDate::Today().FormatISODate();
    const wxString baseCurrencySymbol = Model_Currency::GetBaseCurrency()->CURRENCY_SYMBOL;
    std::map<wxString, double> usedRates;
    const auto currencies = Model_Currency::instance().all();
    int limit = 10;
    for (const auto currency : currencies)
    {
        if (Model_Account::is_used(currency)) {

            double convertionRate = Model_CurrencyHistory::getDayRate(currency.CURRENCYID
                , today);
            usedRates[currency.CURRENCY_SYMBOL] = convertionRate;

            if (--limit <= 0) {
                break;
            }
        }
    }

    if (usedRates.size() == 1) {
        return "";
    }
    wxString header;
    loop_t contents;
    for (const auto i : usedRates)
    {
        row_t r;
        r(L"CURRENCY_SYMBOL") = i.first;
        wxString row;
        for (const auto j : usedRates)
        {
            row += wxString::Format("<td%s>%s</td>" //<td class ='active'>
                , j.first == i.first ? " class ='active'" : ""
                , j.first == i.first ? "" : Model_Currency::toString(
                    j.second / i.second, nullptr, 4)
            );
        }
        header += wxString::Format("<th>%s</th>", i.first);
        r(L"CONVERSION_RATE") = row;

        contents += r;
    }
    mm_html_template report(currencyRatesTemplate);
    report(L"CONTENTS") = contents;
    report(L"FRAME_NAME") = _("Currency Exchange Rates");
    report(L"HEADER") = header;

    wxString out = wxEmptyString;
    try
    {
        out = report.Process();
    }
    catch (const syntax_ex& e)
    {
        return e.what();
    }
    catch (...)
    {
        return _("Caught exception");
    }

    return out;

}

htmlWidgetCurrency::~htmlWidgetCurrency()
{
}
