#include "utils.h"


bool load_test_file(Mat &src, int n)
{
	char filename[50];
	sprintf(filename, "res/ICDAR2015_test/img_%d.jpg", n);
	src = imread(filename, CV_LOAD_IMAGE_UNCHANGED);

	if (src.empty())
	{
		std::cout << n << "\tFail to open" << filename << endl;
		return false;
	}

	else if (src.cols > MAX_WIDTH || src.rows > MAX_HEIGHT)
	{
		std::cout << n << "\t" << src.rows << "," << src.cols << "\tResize the image" << endl;
		double resize_factor = (src.rows > MAX_HEIGHT) ? (double)MAX_HEIGHT / src.rows : (double)MAX_WIDTH / src.cols;

		resize(src, src, Size(src.cols*resize_factor, src.rows*resize_factor));
		return true;
	}

	std::cout << n << "\t" << src.rows << "," << src.cols << endl;
	return true;
}


void show_result(Mat &src, vector<ERs> &pool, vector<ERs> &strong, vector<ERs> &weak, ERs &tracked, vector<Text> &text)
{
	Mat strong_img = src.clone();
	Mat weak_img = src.clone();
	Mat all_img = src.clone();
	Mat tracked_img = src.clone();
	Mat group_result = src.clone();
	for (int i = 0; i < pool.size(); i++)
	{
		for (auto it : weak[i])
			rectangle(weak_img, it->bound, Scalar(0, 0, 255));

		for (auto it : strong[i])
			rectangle(strong_img, it->bound, Scalar(0, 255, 0));

		for (auto it : pool[i])
			rectangle(all_img, it->bound, Scalar(255, 0, 0));
	}

	for (auto it : tracked)
	{
		rectangle(tracked_img, it->bound, Scalar(255, 0, 255));
	}

	for (auto it : text)
	{
		rectangle(group_result, it.box, Scalar(0, 255, 255), 2);
	}

#ifdef DO_OCR
	for (int i = 0; i < text.size(); i++)
	{
		putText(group_result, text[i].word, text[i].box.tl(), FONT_HERSHEY_DUPLEX, 1, Scalar(0, 0, 255), 2);
	}
#endif

	cv::imshow("weak", weak_img);
	cv::imshow("strong", strong_img);
	cv::imshow("all", all_img);
	cv::imshow("tracked", tracked_img);
	cv::imshow("group result", group_result);



#ifndef WEBCAM_MODE
	waitKey(0);
#endif
	

	
}


vector<Vec4i> load_gt(int n)
{
	char filename[50];
	sprintf(filename, "res/ICDAR2015_test_GT/gt_img_%d.txt", n);
	fstream fin(filename, fstream::in);
	if (!fin.is_open())
	{
		std::cout << "Error: Ground Truth file " << n << " is not opened!!" << endl;
		return vector<Vec4i>();
	}

	char picname[50];
	sprintf(picname, "res/ICDAR2015_test/img_%d.jpg", n);
	Mat src = imread(picname, CV_LOAD_IMAGE_UNCHANGED);


	vector<string> data;
	while (!fin.eof())
	{
		string buf;
		fin >> buf;
		data.push_back(buf);
	}

	// the last data would be eof, erase it
	data.pop_back();
	for (int i = 0; i < data.size(); i++)
	{
		data[i].pop_back();
		if (i % 5 == 4)
			data[i].erase(data[i].begin());
	}

	double resize_factor = 1.0;
	if (src.cols > MAX_WIDTH || src.rows > MAX_HEIGHT)
	{
		resize_factor = (src.rows > MAX_HEIGHT) ? (double)MAX_HEIGHT / src.rows : (double)MAX_WIDTH / src.cols;
	}

	// convert string numbers to bounding box, format as shown below
	// 0 0 100 100 HAHA
	// first 2 numbers represent the coordinate of top left point
	// last 2 numbers represent the coordinate of bottom right point
	vector<Rect> bbox;
	for (int i = 0; i < data.size(); i += 5)
	{
		int x1 = stoi(data[i]);
		int y1 = stoi(data[i + 1]);
		int x2 = stoi(data[i + 2]);
		int y2 = stoi(data[i + 3]);

		x1 *= resize_factor;
		y1 *= resize_factor;
		x2 *= resize_factor;
		y2 *= resize_factor;

		bbox.push_back(Rect(Point(x1, y1), Point(x2, y2)));
	}

	// merge the bounding box that could in the same group
	/*for (int i = 0; i < bbox.size(); i++)
	{
		for (int j = i+1; j < bbox.size(); j++)
		{
		if (abs(bbox[i].y - bbox[j].y) < bbox[i].height &&
			abs(bbox[i].y - bbox[j].y) < 0.2 * src.cols * resize_factor &&
			(double)min(bbox[i].height, bbox[j].height) / (double)max(bbox[i].height, bbox[j].height) > 0.7)
			{
				int x1 = min(bbox[i].x, bbox[j].x);
				int y1 = min(bbox[i].y, bbox[j].y);
				int x2 = max(bbox[i].br().x, bbox[j].br().x);
				int y2 = max(bbox[i].br().y, bbox[j].br().y);
				bbox[i] = Rect(Point(x1, y1), Point(x2, y2));
				bbox.erase(bbox.begin() + j);
				j--;
			}
		}
	}*/

	vector<Vec4i> gt;
	for (int i = 0; i < bbox.size(); i++)
	{
		gt.push_back(Vec4i(bbox[i].x, bbox[i].y, bbox[i].width, bbox[i].height));
	}

	fin.close();
	return gt;
}


Vec6d calc_detection_rate(int n, vector<Text> &text)
{
	vector<Vec4i> gt = load_gt(n);
	vector<Rect> gt_box;
	for (int i = 0; i < gt.size(); i++)
	{
		gt_box.push_back(Rect(gt[i][0], gt[i][1], gt[i][2], gt[i][3]));
	}


	vector<bool> detected(gt_box.size());
	double tp = 0;
	for (int i = 0; i < gt_box.size(); i++)
	{
		for (int j = 0; j < text.size(); j++)
		{
			Rect overlap = gt_box[i] & text[j].box;
			Rect union_box = gt_box[i] | text[j].box;

			if ((double)overlap.area() / union_box.area() > 0.3)
			{
				detected[i] = true;
				tp++;
				break;
			}
		}
	}

	double recall = tp / detected.size();
	double precision = (!text.empty()) ? tp / text.size() : 0;
	double harmonic_mean = (precision != 0) ? (recall*precision) * 2 / (recall + precision) : 0;

	return Vec6d(tp, detected.size(), text.size(), recall, precision, harmonic_mean);
}



void save_deteval_xml(vector<vector<Text>> &text)
{
	remove("gt.xml");
	remove("det.xml");
	fstream fgt("gt.xml", fstream::out);
	fstream fdet("det.xml", fstream::out);

	fgt << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl << "<tagset>" << endl;
	fdet << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << endl << "<tagset>" << endl;

	for (int i = 1; i <= 233; i++)
	{
		vector<Vec4i> gt = load_gt(i);
		if (gt.empty()) continue;

		// ground truth
		fgt << "\t<image>" << endl << "\t\t" << "<imageName>img_" << i << ".jpg</imageName>" << endl;
		fgt << "\t\t<taggedRectangles>" << endl;
		for (int j = 0; j < gt.size(); j++)
		{
			fgt << "\t\t\t<taggedRectangle x=\"" << gt[j][0] << "\" y=\"" << gt[j][1] << "\" width=\"" << gt[j][2] << "\" height=\"" << gt[j][3] << "\" offset=\"0\" />" << endl;
		}
		fgt << "\t\t</taggedRectangles>" << endl;
		fgt << "\t</image>" << endl;


		// detections
		fdet << "\t<image>" << endl << "\t\t" << "<imageName>img_" << i << ".jpg</imageName>" << endl;
		fdet << "\t\t<taggedRectangles>" << endl;
		for (int j = 0; j < text[i-1].size(); j++)
		{
			fdet << "\t\t\t<taggedRectangle x=\"" << text[i-1][j].box.x << "\" y=\"" << text[i - 1][j].box.y << "\" width=\"" << text[i - 1][j].box.width << "\" height=\"" << text[i - 1][j].box.height << "\" offset=\"0\" />" << endl;
		}
		fdet << "\t\t</taggedRectangles>" << endl;
		fdet << "\t</image>" << endl;
	}

	fgt << "</tagset>";
	fdet << "</tagset>";
}


//==================================================
//=============== Training Function ================
//==================================================
void train_classifier()
{
	TrainingData *td1 = new TrainingData();
	TrainingData *td2 = new TrainingData();
	AdaBoost adb1(AdaBoost::REAL, AdaBoost::DECISION_STUMP, 30);
	AdaBoost adb2(AdaBoost::REAL, AdaBoost::DECISION_STUMP, 60);


	td1->read_data("er_classifier/training_data.txt");
	adb1.train_classifier(*td1, "er_classifier/adb1.classifier");
	
	for (int i = 0; i < td1->data.size(); i++)
	{
		if (adb1.predict(td1->data[i].fv) < 2.0)
		{
			td2->data.push_back(td1->data[i]);
		}
	}
	
	delete td1;


	td2->set_num(td2->data.size());
	td2->set_dim(td2->data.front().fv.size());
	adb2.train_classifier(*td2, "er_classifier/adb2.classifier");
}


void train_cascade()
{
	double Ftarget1 = 0.02;
	double f1 = 0.80;
	double d1 = 0.80;
	double Ftarget2 = 0.30;
	double f2 = 0.90;
	double d2 = 0.90;
	TrainingData *td1 = new TrainingData();
	TrainingData *tmp = new TrainingData();
	TrainingData *td2 = new TrainingData();
	AdaBoost *adb1 = new CascadeBoost(AdaBoost::REAL, AdaBoost::DECISION_STUMP, Ftarget1, f1, d1);
	AdaBoost *adb2 = new CascadeBoost(AdaBoost::REAL, AdaBoost::DECISION_STUMP, Ftarget2, f2, d2);

	freopen("er_classifier/log.txt", "w", stdout);

	cout << "Strong Text    Ftarget:" << Ftarget1 << " f=" << f1 << " d:" << d1 << endl;
	td1->read_data("er_classifier/training_data.txt");
	adb1->train_classifier(*td1, "er_classifier/cascade1.classifier");

	cout << endl << "Weak Text    Ftarget:" << Ftarget2 << " f=" << f2 << " d:" << d2 << endl;
	td2->read_data("er_classifier/training_data.txt");
	adb2->train_classifier(*td2, "er_classifier/cascade2.classifier");
}


void bootstrap()
{
	ERFilter *erFilter = new ERFilter(THRESHOLD_STEP, MIN_ER_AREA, MAX_ER_AREA, NMS_STABILITY_T, NMS_OVERLAP_COEF);
	erFilter->adb1 = new CascadeBoost("er_classifier/cascade1.classifier");
	erFilter->adb2 = new CascadeBoost("er_classifier/cascade2.classifier");


	int i = 0;
	for (int pic = 1; pic <= 10000; pic++)
	{
		char filename[30];
		sprintf(filename, "res/neg4/%d.jpg", pic);

		ERs pool, strong, weak;
		Mat input = imread(filename, IMREAD_GRAYSCALE);
		if (input.empty()) continue;


		ER *root = erFilter->er_tree_extract(input);
		erFilter->non_maximum_supression(root, pool, input);
		erFilter->classify(pool, strong, weak, input);


		/*for (auto it : strong)
		{
			char imgname[30];
			sprintf(imgname, "res/tmp1/%d_%d.jpg", pic, i);
			imwrite(imgname, input(it->bound));
			i++;
		}*/

		for (auto it : weak)
		{
			char imgname[30];
			sprintf(imgname, "res/tmp1/%d_%d.jpg", pic, i);
			imwrite(imgname, input(it->bound));
			i++;
		}

		cout << pic << " finish " << endl;
	}
}



void get_canny_data()
{
	fstream fout = fstream("er_classifier/training_data.txt", fstream::out);

	ERFilter erFilter(THRESHOLD_STEP, MIN_ER_AREA, MAX_ER_AREA, NMS_STABILITY_T, NMS_OVERLAP_COEF);
	erFilter.ocr = new OCR();

	const int N = 2;
	const int normalize_size = 24;

	for (int i = 1; i <= 4; i++)
	{
		for (int pic = 1; pic <= 10000; pic++)
		{
			char filename[30];
			sprintf(filename, "res/neg%d/%d.jpg", i, pic);

			Mat input = imread(filename, IMREAD_GRAYSCALE);
			if (input.empty())	continue;

			vector<double> spacial_hist = erFilter.make_LBP_hist(input, N, normalize_size);
			fout << -1;
			for (int f = 0; f < spacial_hist.size(); f++)
			{
				if(spacial_hist[f])
					fout << " " << f << ":" << spacial_hist[f];
			}
			fout << endl;

			/*spacial_hist = erFilter.make_LBP_hist(255-input, N, normalize_size);
			fout << -1;
			for (int f = 0; f < spacial_hist.size(); f++)
				fout << " " << spacial_hist[f];
			fout << endl;*/


			cout << pic << "\tneg" << i << " finish " << endl;
		}
	}
	


	for (int i = 1; i <= 4; i++)
	{
		for (int pic = 1; pic <= 10000; pic++)
		{
			char filename[30];
			sprintf(filename, "res/pos%d/%d.jpg", i, pic);

			Mat input = imread(filename, IMREAD_GRAYSCALE);
			if (input.empty())	continue;

			vector<double> spacial_hist = erFilter.make_LBP_hist(input, N, normalize_size);
			fout << 1;
			for (int f = 0; f < spacial_hist.size(); f++)
			{
				if (spacial_hist[f])
					fout << " " << f << ":" << spacial_hist[f];
			}
			fout << endl;

			spacial_hist = erFilter.make_LBP_hist(255 - input, N, normalize_size);
			fout << 1;
			for (int f = 0; f < spacial_hist.size(); f++)
			{
				if (spacial_hist[f])
					fout << " " << f << ":" << spacial_hist[f];
			}
			fout << endl;

			cout << pic << "\tpos" << i <<" finish " << endl;
		}
	}
	
}


void get_ocr_data(int argc, char **argv, int type)
{
	char *in_img = nullptr;
	char *outfile = nullptr;
	int label = 0;
	if (argc != 4)
	{
		cout << "wrong input format" << endl;
		return;
	}

	else
	{
		in_img = argv[1];
		outfile = argv[2];
		label = atoi(argv[3]);
	}



	Mat input = imread(in_img, IMREAD_GRAYSCALE);
	if (input.empty())
	{
		cout << "No such file:" << in_img << endl;
		return;
	}


	ERFilter erFilter(THRESHOLD_STEP, MIN_ER_AREA, MAX_ER_AREA, NMS_STABILITY_T, NMS_OVERLAP_COEF);
	erFilter.ocr = new OCR();

	fstream fout = fstream(outfile, fstream::app);
	fout << label;

	if (type == 0)
	{
		Mat ocr_img;
		threshold(255 - input, ocr_img, 128, 255, CV_THRESH_OTSU);
		erFilter.ocr->rotate_mat(ocr_img, ocr_img, 0, true);
		erFilter.ocr->ARAN(ocr_img, ocr_img, 35);

		svm_node *fv = new svm_node[201];
		erFilter.ocr->extract_feature(ocr_img, fv);

		int i = 0;
		while (fv[i].index != -1)
		{
			fout << " " << fv[i].index << ":" << fv[i].value;
			i++;
		}

		fout << endl;
	}

	else if (type == 1)
	{
		const int N = 2;
		const int normalize_size = 24;

		vector<double> spacial_hist = erFilter.make_LBP_hist(input, N, normalize_size);

		double scale = (normalize_size / 2) * (normalize_size / 2);
		for (int f = 0; f < spacial_hist.size(); f++)
		{
			if (spacial_hist[f] != 0)
				fout << " " << f << ":" << spacial_hist[f] / 129.0;
		}

		fout << endl;
	}

	return;
}


void opencv_train()
{
	using namespace ml;
	Ptr<Boost> boost = Boost::create();
	Ptr<TrainData> trainData = TrainData::loadFromCSV("er_classifier/training_data.txt", 0, 0, 1, String(), ' ');
	boost->setBoostType(Boost::REAL);
	boost->setWeakCount(100);
	boost->setMaxDepth(1);
	boost->setWeightTrimRate(0);
	cout << "training..." << endl;
	boost->train(trainData);
	boost->save("er_classifier/opencv_classifier.xml");
}