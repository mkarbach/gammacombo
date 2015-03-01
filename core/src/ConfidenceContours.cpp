#include "ConfidenceContours.h"

ConfidenceContours::ConfidenceContours(OptParser *arg)
{
  assert(arg);
  m_arg = arg;
}

ConfidenceContours::~ConfidenceContours()
{}

///
/// Helper function for computeContours():
/// Constructs a new 2D histogram that contains 2 more bins in each
/// direction that are set to 0, so that the contours will always close.
///
/// \param hist - the 2D histogram
/// \return new 2D histogram. Caller assumes ownership.
///
TH2F* ConfidenceContours::addBoundaryBins(TH2F* hist)
{
	float boundary = hist->GetMinimum();
	TH2F* hBoundaries = new TH2F(getUniqueRootName(),getUniqueRootName(),
			hist->GetNbinsX()+2,
			hist->GetXaxis()->GetXmin() - hist->GetXaxis()->GetBinWidth(1),
			hist->GetXaxis()->GetXmax() + hist->GetXaxis()->GetBinWidth(1),
			hist->GetNbinsY()+2,
			hist->GetYaxis()->GetXmin() - hist->GetYaxis()->GetBinWidth(1),
			hist->GetYaxis()->GetXmax() + hist->GetYaxis()->GetBinWidth(1));
	for ( int ix=1; ix<=hBoundaries->GetXaxis()->GetNbins(); ix++ ){
		for ( int iy=1; iy<=hBoundaries->GetYaxis()->GetNbins(); iy++ ){
			if ( ix==1 || ix==hBoundaries->GetXaxis()->GetNbins()
					|| iy==1 || iy==hBoundaries->GetYaxis()->GetNbins() )
				hBoundaries->SetBinContent(ix,iy,boundary);
			else{
				hBoundaries->SetBinContent(ix,iy,hist->GetBinContent(ix-1,iy-1));
			}
		}
	}
	return hBoundaries;
}

///
/// Helper function for computeContours():
/// Transforms the chi2 valley into a hill to help ROOTs contour mechanism
///
/// \param hist - the 2D histogram
///
void ConfidenceContours::transformChi2valleyToHill(TH2F* hist, float offset)
{
	float chi2min = hist->GetMinimum();
	//cout << "ConfidenceContours::transformChi2valleyToHill() : chi2min=" << chi2min << endl;
	for ( int ix=1; ix<=hist->GetXaxis()->GetNbins(); ix++ ){
		for ( int iy=1; iy<=hist->GetYaxis()->GetNbins(); iy++ ){
			hist->SetBinContent(ix,iy,-hist->GetBinContent(ix,iy)+offset+chi2min);
		}
	}
}

///
/// Compute the raw N sigma confidence contours from a 2D histogram
/// holding either the chi2 or the p-value curve. The resulting
/// contours are stored into the m_contours member.
///
/// \param hist - the 2D histogram
/// \param type - the type of the 2D histogram, either chi2 or p-value
///
void ConfidenceContours::computeContours(TH2F* hist, histogramType type)
{
	if ( m_arg->debug ) {
		cout << "ConfidenceContours::computeContours() : making contours of histogram ";
		cout << hist->GetName();
		cout << ", type " << (type==kChi2?"chi2":"p-value") << endl;
	}
	// clean up contours from a previous call
	m_contours.clear();

	// transform chi2 from valley to hill
	float offset = 30.;
	if ( type==kChi2 ) transformChi2valleyToHill(hist,offset);

	// add boundaries
	TH2F* histb = addBoundaryBins(hist);

	// make contours
	const int nMaxContours = 5;
	histb->SetContour(nMaxContours);
	if ( type==kChi2 ) {
		// chi2 units
		if ( m_arg->plot2dcl ){
			histb->SetContourLevel(4, offset- 2.30);
			histb->SetContourLevel(3, offset- 6.18);
			histb->SetContourLevel(2, offset-11.83);
			histb->SetContourLevel(1, offset-19.34);
			histb->SetContourLevel(0, offset-28.76);
		}
		else{
			histb->SetContourLevel(4, offset-1.);
			histb->SetContourLevel(3, offset-4.);
			histb->SetContourLevel(2, offset-9.);
			histb->SetContourLevel(1, offset-16.);
			histb->SetContourLevel(0, offset-25.);
		}
	}
	else {
		// p-value units
		histb->SetContourLevel(4, 0.3173);
		histb->SetContourLevel(3, 4.55e-2);
		histb->SetContourLevel(2, 2.7e-3);
		histb->SetContourLevel(1, 6.3e-5);
		histb->SetContourLevel(0, 5.7e-7);
	}

	// create and access the contours
	gROOT->SetBatch(true); // don't display the temporary canvas
	TCanvas *ctmp = new TCanvas(getUniqueRootName(), "ctmp");
	histb->Draw("contlist");
	gPad->Update();	// needed to be able to access the contours as TGraphs
	TObjArray *contours = (TObjArray*)gROOT->GetListOfSpecials()->FindObject("contours");
	delete ctmp;
	delete histb;
	gROOT->SetBatch(false);

	// access contours. They get filled in reverse order,
	// and depend on how many are actually present. If all 5
	// are filled, index 0 is 5sigma. If only 2 are filled, index 0
	// is 2 sigma.
	int nEmptyContours = 0;
	for ( int ic=4; ic>=0; ic-- ){
		if (((TList*)contours->At(ic))->IsEmpty()) nEmptyContours++;
	}
	for ( int ic=4; ic>=0; ic-- ){
		if ( !(((TList*)contours->At(ic))->IsEmpty()) ){
			Contour* cont = new Contour(m_arg, (TList*)contours->At(ic));
			cont->setSigma(5-nEmptyContours-ic);
			m_contours.push_back(cont);
		}
	}

	// magnetic boundaries
	if ( m_arg->plotmagnetic ) {
		for ( int ic=4; ic>=0; ic-- ){
			m_contours[ic]->magneticBoundaries(hist);
		}
	}
}

///
/// Draw the contours into the currently active Canvas.
///
void ConfidenceContours::Draw()
{
	//cout << "ConfidenceContours::Draw() : drawing ..." << endl;
	for ( int i=m_arg->plotnsigmacont-1; i>=0; i-- ) {
		m_contours[i]->setStyle(m_linecolor[i], m_linestyle[i], m_linewidth[i], m_fillcolor[i], m_fillstyle[i]);
		m_contours[i]->Draw();
	}
}

///
/// Draw the contours into the currently active Canvas.
///
void ConfidenceContours::DrawDashedLine()
{
	//cout << "ConfidenceContours::DrawDashedLine() : drawing ..." << endl;
	for ( int i=m_arg->plotnsigmacont-1; i>=0; i-- ) {
		m_contours[i]->setStyle(m_linecolor[i], kDashed, m_linewidth[i], 0, 0);
		m_contours[i]->DrawLine();
	}
}

///
/// Set the contour style.
///
void ConfidenceContours::setStyle(vector<int>& linecolor, vector<int>& linestyle, vector<int>& fillcolor, vector<int>& fillstyle)
{
	m_linecolor = linecolor;
	m_linestyle = linestyle;
	m_fillcolor = fillcolor;
	m_fillstyle = fillstyle;
	for ( int i=0; i<m_linestyle.size(); i++ ) m_linewidth.push_back(2);
	// check if enough styles where given for the number of contours to be plotted
	if ( m_arg->plotnsigmacont > m_linestyle.size() ){
		cout << "ConfidenceContours::setStyle() : ERROR : not enough sigma contour styles defined! ";
		cout << "Reusing style of " << m_linestyle.size() << " sigma contour." << endl;
		for ( int i=m_linestyle.size(); i<m_arg->plotnsigmacont; i++ ){
			int laststyle = m_linestyle.size()-1;
			m_linecolor.push_back(m_linecolor[laststyle]);
			m_linestyle.push_back(m_linestyle[laststyle]);
			m_fillcolor.push_back(m_fillcolor[laststyle]);
			m_fillstyle.push_back(m_fillstyle[laststyle]);
			m_linewidth.push_back(m_linewidth[laststyle]);
		}
	}
}